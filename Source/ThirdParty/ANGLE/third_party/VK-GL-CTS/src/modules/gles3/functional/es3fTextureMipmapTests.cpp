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
 * \brief Mipmapping tests.
 *//*--------------------------------------------------------------------*/

#include "es3fTextureMipmapTests.hpp"

#include "glsTextureTestUtil.hpp"
#include "gluTexture.hpp"
#include "gluTextureUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuMatrix.hpp"
#include "tcuMatrixUtil.hpp"
#include "tcuTexLookupVerifier.hpp"
#include "tcuVectorUtil.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "deString.h"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

using std::string;
using std::vector;
using namespace deqp::gls;

namespace deqp
{
namespace gles3
{
namespace Functional
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

static float getMinLodForCell(int cellNdx)
{
    static const float s_values[] = {1.0f, 3.5f, 2.0f,  -2.0f, 0.0f, 3.0f, 10.0f, 4.8f,
                                     5.8f, 5.7f, -1.9f, 4.0f,  6.5f, 7.1f, -1e10, 1000.f};
    return s_values[cellNdx % DE_LENGTH_OF_ARRAY(s_values)];
}

static float getMaxLodForCell(int cellNdx)
{
    static const float s_values[] = {0.0f, 0.2f, 0.7f, 0.4f, 1.3f, 0.0f,  0.5f,    1.2f, -2.0f,
                                     1.0f, 0.1f, 0.3f, 2.7f, 1.2f, 10.0f, -1000.f, 1e10f};
    return s_values[cellNdx % DE_LENGTH_OF_ARRAY(s_values)];
}

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
    : TestCase(testCtx, name, desc)
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
    if (m_coordType == COORDTYPE_PROJECTED && m_renderCtx.getRenderTarget().getNumSamples() > 0)
        throw tcu::NotSupportedError("Projected lookup validation not supported in multisample config");

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
    const glw::Functions &gl = m_renderCtx.getFunctions();

    const tcu::Texture2D &refTexture = m_texture->getRefTexture();

    const uint32_t magFilter    = GL_NEAREST;
    const int texWidth          = refTexture.getWidth();
    const int texHeight         = refTexture.getHeight();
    const int defViewportWidth  = texWidth * 4;
    const int defViewportHeight = texHeight * 4;

    const RandomViewport viewport(m_renderCtx.getRenderTarget(), defViewportWidth, defViewportHeight,
                                  deStringHash(getName()));
    ReferenceParams sampleParams(TEXTURETYPE_2D);
    vector<float> texCoord;

    const bool isProjected = m_coordType == COORDTYPE_PROJECTED;
    const bool useLodBias  = m_coordType == COORDTYPE_BASIC_BIAS;

    tcu::Surface renderedFrame(viewport.width, viewport.height);

    // Viewport is divided into 4x4 grid.
    int gridWidth  = 4;
    int gridHeight = 4;
    int cellWidth  = viewport.width / gridWidth;
    int cellHeight = viewport.height / gridHeight;

    // Bail out if rendertarget is too small.
    if (viewport.width < defViewportWidth / 2 || viewport.height < defViewportHeight / 2)
        throw tcu::NotSupportedError("Too small viewport", "", __FILE__, __LINE__);

    // Sampling parameters.
    sampleParams.sampler     = glu::mapGLSampler(m_wrapS, m_wrapT, m_minFilter, magFilter);
    sampleParams.samplerType = glu::TextureTestUtil::getSamplerType(m_texture->getRefTexture().getFormat());
    sampleParams.flags = (isProjected ? ReferenceParams::PROJECTED : 0) | (useLodBias ? ReferenceParams::USE_BIAS : 0);
    sampleParams.lodMode = LODMODE_EXACT; // Use ideal lod.

    // Upload texture data.
    m_texture->upload();

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
            const int curX    = cellWidth * gridX;
            const int curY    = cellHeight * gridY;
            const int curW    = gridX + 1 == gridWidth ? (viewport.width - curX) : cellWidth;
            const int curH    = gridY + 1 == gridHeight ? (viewport.height - curY) : cellHeight;
            const int cellNdx = gridY * gridWidth + gridX;

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
        }
    }

    // Read result.
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());

    // Compare and log.
    {
        const tcu::PixelFormat &pixelFormat = m_renderCtx.getRenderTarget().getPixelFormat();
        const bool isTrilinear = m_minFilter == GL_NEAREST_MIPMAP_LINEAR || m_minFilter == GL_LINEAR_MIPMAP_LINEAR;
        tcu::Surface referenceFrame(viewport.width, viewport.height);
        tcu::Surface errorMask(viewport.width, viewport.height);
        tcu::LookupPrecision lookupPrec;
        tcu::LodPrecision lodPrec;
        int numFailedPixels = 0;

        lookupPrec.coordBits = tcu::IVec3(20, 20, 0);
        lookupPrec.uvwBits   = tcu::IVec3(16, 16, 0); // Doesn't really matter since pixels are unicolored.
        lookupPrec.colorThreshold =
            tcu::computeFixedPointThreshold(max(getBitsVec(pixelFormat) - (isTrilinear ? 2 : 1), tcu::IVec4(0)));
        lookupPrec.colorMask = getCompareMask(pixelFormat);
        lodPrec.derivateBits = 10;
        lodPrec.lodBits      = isProjected ? 6 : 8;

        for (int gridY = 0; gridY < gridHeight; gridY++)
        {
            for (int gridX = 0; gridX < gridWidth; gridX++)
            {
                const int curX    = cellWidth * gridX;
                const int curY    = cellHeight * gridY;
                const int curW    = gridX + 1 == gridWidth ? (viewport.width - curX) : cellWidth;
                const int curH    = gridY + 1 == gridHeight ? (viewport.height - curY) : cellHeight;
                const int cellNdx = gridY * gridWidth + gridX;

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

                // Render ideal result
                sampleTexture(tcu::SurfaceAccess(referenceFrame, pixelFormat, curX, curY, curW, curH), refTexture,
                              &texCoord[0], sampleParams);

                // Compare this cell
                numFailedPixels += computeTextureLookupDiff(
                    tcu::getSubregion(renderedFrame.getAccess(), curX, curY, curW, curH),
                    tcu::getSubregion(referenceFrame.getAccess(), curX, curY, curW, curH),
                    tcu::getSubregion(errorMask.getAccess(), curX, curY, curW, curH), m_texture->getRefTexture(),
                    &texCoord[0], sampleParams, lookupPrec, lodPrec, m_testCtx.getWatchDog());
            }
        }

        if (numFailedPixels > 0)
            m_testCtx.getLog() << TestLog::Message << "ERROR: Image verification failed, found " << numFailedPixels
                               << " invalid pixels!" << TestLog::EndMessage;

        m_testCtx.getLog() << TestLog::ImageSet("Result", "Verification result")
                           << TestLog::Image("Rendered", "Rendered image", renderedFrame);

        if (numFailedPixels > 0)
        {
            m_testCtx.getLog() << TestLog::Image("Reference", "Ideal reference", referenceFrame)
                               << TestLog::Image("ErrorMask", "Error mask", errorMask);
        }

        m_testCtx.getLog() << TestLog::EndImageSet;

        {
            const bool isOk = numFailedPixels == 0;
            m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                    isOk ? "Pass" : "Image verification failed");
        }
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
    : TestCase(testCtx, name, desc)
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
    if (m_coordType == COORDTYPE_PROJECTED && m_renderCtx.getRenderTarget().getNumSamples() > 0)
        throw tcu::NotSupportedError("Projected lookup validation not supported in multisample config");

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
    const uint32_t magFilter    = GL_NEAREST;
    const int texWidth          = m_texture->getRefTexture().getSize();
    const int texHeight         = m_texture->getRefTexture().getSize();
    const int defViewportWidth  = texWidth * 2;
    const int defViewportHeight = texHeight * 2;

    const glw::Functions &gl = m_renderCtx.getFunctions();
    const RandomViewport viewport(m_renderCtx.getRenderTarget(), defViewportWidth, defViewportHeight,
                                  deStringHash(getName()));

    const bool isProjected = m_coordType == COORDTYPE_PROJECTED;
    const bool useLodBias  = m_coordType == COORDTYPE_BASIC_BIAS;

    vector<float> texCoord;
    tcu::Surface renderedFrame(viewport.width, viewport.height);

    // Bail out if rendertarget is too small.
    if (viewport.width < defViewportWidth / 2 || viewport.height < defViewportHeight / 2)
        throw tcu::NotSupportedError("Too small viewport", "", __FILE__, __LINE__);

    // Upload texture data.
    m_texture->upload();

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

    // Render with GL
    for (int cellNdx = 0; cellNdx < (int)gridLayout.size(); cellNdx++)
    {
        const int curX               = gridLayout[cellNdx].x();
        const int curY               = gridLayout[cellNdx].y();
        const int curW               = gridLayout[cellNdx].z();
        const int curH               = gridLayout[cellNdx].w();
        const tcu::CubeFace cubeFace = (tcu::CubeFace)(cellNdx % tcu::CUBEFACE_LAST);
        RenderParams params(TEXTURETYPE_CUBE);

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
    }
    GLU_EXPECT_NO_ERROR(gl.getError(), "Draw");

    // Read result.
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());
    GLU_EXPECT_NO_ERROR(gl.getError(), "Read pixels");

    // Render reference and compare
    {
        tcu::Surface referenceFrame(viewport.width, viewport.height);
        tcu::Surface errorMask(viewport.width, viewport.height);
        int numFailedPixels = 0;
        ReferenceParams params(TEXTURETYPE_CUBE);
        tcu::LookupPrecision lookupPrec;
        tcu::LodPrecision lodPrec;

        // Params for rendering reference
        params.sampler                 = glu::mapGLSampler(m_wrapS, m_wrapT, m_minFilter, magFilter);
        params.sampler.seamlessCubeMap = true;
        params.lodMode                 = LODMODE_EXACT;

        // Comparison parameters
        lookupPrec.colorMask      = getCompareMask(m_renderCtx.getRenderTarget().getPixelFormat());
        lookupPrec.colorThreshold = tcu::computeFixedPointThreshold(
            max(getBitsVec(m_renderCtx.getRenderTarget().getPixelFormat()) - (isProjected ? 3 : 2), IVec4(0)));
        lookupPrec.coordBits = isProjected ? tcu::IVec3(8) : tcu::IVec3(10);
        lookupPrec.uvwBits   = tcu::IVec3(5, 5, 0);
        lodPrec.derivateBits = 10;
        lodPrec.lodBits      = isProjected ? 3 : 6;

        for (int cellNdx = 0; cellNdx < (int)gridLayout.size(); cellNdx++)
        {
            const int curX               = gridLayout[cellNdx].x();
            const int curY               = gridLayout[cellNdx].y();
            const int curW               = gridLayout[cellNdx].z();
            const int curH               = gridLayout[cellNdx].w();
            const tcu::CubeFace cubeFace = (tcu::CubeFace)(cellNdx % tcu::CUBEFACE_LAST);

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

            // Render ideal reference.
            {
                tcu::SurfaceAccess idealDst(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat(), curX, curY,
                                            curW, curH);
                sampleTexture(idealDst, m_texture->getRefTexture(), &texCoord[0], params);
            }

            // Compare this cell
            numFailedPixels += computeTextureLookupDiff(
                tcu::getSubregion(renderedFrame.getAccess(), curX, curY, curW, curH),
                tcu::getSubregion(referenceFrame.getAccess(), curX, curY, curW, curH),
                tcu::getSubregion(errorMask.getAccess(), curX, curY, curW, curH), m_texture->getRefTexture(),
                &texCoord[0], params, lookupPrec, lodPrec, m_testCtx.getWatchDog());
        }

        if (numFailedPixels > 0)
            m_testCtx.getLog() << TestLog::Message << "ERROR: Image verification failed, found " << numFailedPixels
                               << " invalid pixels!" << TestLog::EndMessage;

        m_testCtx.getLog() << TestLog::ImageSet("Result", "Verification result")
                           << TestLog::Image("Rendered", "Rendered image", renderedFrame);

        if (numFailedPixels > 0)
        {
            m_testCtx.getLog() << TestLog::Image("Reference", "Ideal reference", referenceFrame)
                               << TestLog::Image("ErrorMask", "Error mask", errorMask);
        }

        m_testCtx.getLog() << TestLog::EndImageSet;

        {
            const bool isOk = numFailedPixels == 0;
            m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                    isOk ? "Pass" : "Image verification failed");
        }
    }

    return STOP;
}

// Texture2DGenMipmapCase

class Texture2DGenMipmapCase : public tcu::TestCase
{
public:
    Texture2DGenMipmapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name, const char *desc,
                           uint32_t format, uint32_t dataType, uint32_t hint, int width, int height);
    ~Texture2DGenMipmapCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture2DGenMipmapCase(const Texture2DGenMipmapCase &other);
    Texture2DGenMipmapCase &operator=(const Texture2DGenMipmapCase &other);

    glu::RenderContext &m_renderCtx;

    uint32_t m_format;
    uint32_t m_dataType;
    uint32_t m_hint;
    int m_width;
    int m_height;

    glu::Texture2D *m_texture;
    TextureRenderer m_renderer;
};

Texture2DGenMipmapCase::Texture2DGenMipmapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                               const char *name, const char *desc, uint32_t format, uint32_t dataType,
                                               uint32_t hint, int width, int height)
    : TestCase(testCtx, name, desc)
    , m_renderCtx(renderCtx)
    , m_format(format)
    , m_dataType(dataType)
    , m_hint(hint)
    , m_width(width)
    , m_height(height)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
{
}

Texture2DGenMipmapCase::~Texture2DGenMipmapCase(void)
{
    deinit();
}

void Texture2DGenMipmapCase::init(void)
{
    DE_ASSERT(!m_texture);
    m_texture = new glu::Texture2D(m_renderCtx, m_format, m_dataType, m_width, m_height);
}

void Texture2DGenMipmapCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

Texture2DGenMipmapCase::IterateResult Texture2DGenMipmapCase::iterate(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    const uint32_t minFilter = GL_NEAREST_MIPMAP_NEAREST;
    const uint32_t magFilter = GL_NEAREST;
    const uint32_t wrapS     = GL_CLAMP_TO_EDGE;
    const uint32_t wrapT     = GL_CLAMP_TO_EDGE;

    const int numLevels = deLog2Floor32(de::max(m_width, m_height)) + 1;

    tcu::Texture2D resultTexture(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                                 m_texture->getRefTexture().getWidth(), m_texture->getRefTexture().getHeight());

    vector<float> texCoord;

    // Initialize texture level 0 with colored grid.
    m_texture->getRefTexture().allocLevel(0);
    tcu::fillWithGrid(m_texture->getRefTexture().getLevel(0), 8, tcu::Vec4(1.0f, 0.5f, 0.0f, 0.5f),
                      tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

    // Upload data and setup params.
    m_texture->upload();

    gl.bindTexture(GL_TEXTURE_2D, m_texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    GLU_EXPECT_NO_ERROR(gl.getError(), "After texture setup");

    // Generate mipmap.
    gl.hint(GL_GENERATE_MIPMAP_HINT, m_hint);
    gl.generateMipmap(GL_TEXTURE_2D);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap()");

    // Use (0, 0) -> (1, 1) texture coordinates.
    computeQuadTexCoord2D(texCoord, Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f));

    // Fetch resulting texture by rendering.
    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        const int levelWidth  = de::max(1, m_width >> levelNdx);
        const int levelHeight = de::max(1, m_height >> levelNdx);
        const RandomViewport viewport(m_renderCtx.getRenderTarget(), levelWidth, levelHeight,
                                      deStringHash(getName()) + levelNdx);

        gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);
        m_renderer.renderQuad(0, &texCoord[0], TEXTURETYPE_2D);

        resultTexture.allocLevel(levelNdx);
        glu::readPixels(m_renderCtx, viewport.x, viewport.y, resultTexture.getLevel(levelNdx));
    }

    // Compare results
    {
        const IVec4 framebufferBits = max(getBitsVec(m_renderCtx.getRenderTarget().getPixelFormat()) - 2, IVec4(0));
        const IVec4 formatBits      = tcu::getTextureFormatBitDepth(glu::mapGLTransferFormat(m_format, m_dataType));
        const tcu::BVec4 formatMask = greaterThan(formatBits, IVec4(0));
        const IVec4 cmpBits         = select(min(framebufferBits, formatBits), framebufferBits, formatMask);
        GenMipmapPrecision comparePrec;

        comparePrec.colorMask      = getCompareMask(m_renderCtx.getRenderTarget().getPixelFormat());
        comparePrec.colorThreshold = tcu::computeFixedPointThreshold(cmpBits);
        comparePrec.filterBits     = tcu::IVec3(4, 4, 0);

        const qpTestResult compareResult =
            compareGenMipmapResult(m_testCtx.getLog(), resultTexture, m_texture->getRefTexture(), comparePrec);

        m_testCtx.setTestResult(compareResult, compareResult == QP_TEST_RESULT_PASS ? "Pass" :
                                               compareResult == QP_TEST_RESULT_QUALITY_WARNING ?
                                                                                      "Low-quality method used" :
                                               compareResult == QP_TEST_RESULT_FAIL ? "Image comparison failed" :
                                                                                      "");
    }

    return STOP;
}

// TextureCubeGenMipmapCase

class TextureCubeGenMipmapCase : public tcu::TestCase
{
public:
    TextureCubeGenMipmapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                             const char *desc, uint32_t format, uint32_t dataType, uint32_t hint, int size);
    ~TextureCubeGenMipmapCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    TextureCubeGenMipmapCase(const TextureCubeGenMipmapCase &other);
    TextureCubeGenMipmapCase &operator=(const TextureCubeGenMipmapCase &other);

    glu::RenderContext &m_renderCtx;

    uint32_t m_format;
    uint32_t m_dataType;
    uint32_t m_hint;
    int m_size;

    glu::TextureCube *m_texture;
    TextureRenderer m_renderer;
};

TextureCubeGenMipmapCase::TextureCubeGenMipmapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                   const char *name, const char *desc, uint32_t format,
                                                   uint32_t dataType, uint32_t hint, int size)
    : TestCase(testCtx, name, desc)
    , m_renderCtx(renderCtx)
    , m_format(format)
    , m_dataType(dataType)
    , m_hint(hint)
    , m_size(size)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
{
}

TextureCubeGenMipmapCase::~TextureCubeGenMipmapCase(void)
{
    deinit();
}

void TextureCubeGenMipmapCase::init(void)
{
    if (m_renderCtx.getRenderTarget().getWidth() < 3 * m_size || m_renderCtx.getRenderTarget().getHeight() < 2 * m_size)
        throw tcu::NotSupportedError("Render target size must be at least (" + de::toString(3 * m_size) + ", " +
                                     de::toString(2 * m_size) + ")");

    DE_ASSERT(!m_texture);
    m_texture = new glu::TextureCube(m_renderCtx, m_format, m_dataType, m_size);
}

void TextureCubeGenMipmapCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

TextureCubeGenMipmapCase::IterateResult TextureCubeGenMipmapCase::iterate(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    const uint32_t minFilter = GL_NEAREST_MIPMAP_NEAREST;
    const uint32_t magFilter = GL_NEAREST;
    const uint32_t wrapS     = GL_CLAMP_TO_EDGE;
    const uint32_t wrapT     = GL_CLAMP_TO_EDGE;

    tcu::TextureCube resultTexture(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                                   m_size);

    const int numLevels = deLog2Floor32(m_size) + 1;
    vector<float> texCoord;

    // Initialize texture level 0 with colored grid.
    for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
    {
        Vec4 ca, cb; // Grid colors.

        switch (face)
        {
        case 0:
            ca = Vec4(1.0f, 0.3f, 0.0f, 0.7f);
            cb = Vec4(0.0f, 0.0f, 1.0f, 1.0f);
            break;
        case 1:
            ca = Vec4(0.0f, 1.0f, 0.5f, 0.5f);
            cb = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
            break;
        case 2:
            ca = Vec4(0.7f, 0.0f, 1.0f, 0.3f);
            cb = Vec4(0.0f, 1.0f, 0.0f, 1.0f);
            break;
        case 3:
            ca = Vec4(0.0f, 0.3f, 1.0f, 1.0f);
            cb = Vec4(1.0f, 0.0f, 0.0f, 0.7f);
            break;
        case 4:
            ca = Vec4(1.0f, 0.0f, 0.5f, 1.0f);
            cb = Vec4(0.0f, 1.0f, 0.0f, 0.5f);
            break;
        case 5:
            ca = Vec4(0.7f, 1.0f, 0.0f, 1.0f);
            cb = Vec4(0.0f, 0.0f, 1.0f, 0.3f);
            break;
        }

        m_texture->getRefTexture().allocLevel((tcu::CubeFace)face, 0);
        fillWithGrid(m_texture->getRefTexture().getLevelFace(0, (tcu::CubeFace)face), 8, ca, cb);
    }

    // Upload data and setup params.
    m_texture->upload();

    gl.bindTexture(GL_TEXTURE_CUBE_MAP, m_texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, wrapS);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, wrapT);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, minFilter);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, magFilter);
    GLU_EXPECT_NO_ERROR(gl.getError(), "After texture setup");

    // Generate mipmap.
    gl.hint(GL_GENERATE_MIPMAP_HINT, m_hint);
    gl.generateMipmap(GL_TEXTURE_CUBE_MAP);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap()");

    // Render all levels.
    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        const int levelWidth  = de::max(1, m_size >> levelNdx);
        const int levelHeight = de::max(1, m_size >> levelNdx);

        for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; faceNdx++)
        {
            const RandomViewport viewport(m_renderCtx.getRenderTarget(), levelWidth * 3, levelHeight * 2,
                                          deStringHash(getName()) ^ deInt32Hash(levelNdx + faceNdx));
            const tcu::CubeFace face = tcu::CubeFace(faceNdx);

            computeQuadTexCoordCube(texCoord, face);

            gl.viewport(viewport.x, viewport.y, levelWidth, levelHeight);
            m_renderer.renderQuad(0, &texCoord[0], TEXTURETYPE_CUBE);

            resultTexture.allocLevel(face, levelNdx);
            glu::readPixels(m_renderCtx, viewport.x, viewport.y, resultTexture.getLevelFace(levelNdx, face));
        }
    }

    // Compare results
    {
        const IVec4 framebufferBits = max(getBitsVec(m_renderCtx.getRenderTarget().getPixelFormat()) - 2, IVec4(0));
        const IVec4 formatBits      = tcu::getTextureFormatBitDepth(glu::mapGLTransferFormat(m_format, m_dataType));
        const tcu::BVec4 formatMask = greaterThan(formatBits, IVec4(0));
        const IVec4 cmpBits         = select(min(framebufferBits, formatBits), framebufferBits, formatMask);
        GenMipmapPrecision comparePrec;

        comparePrec.colorMask      = getCompareMask(m_renderCtx.getRenderTarget().getPixelFormat());
        comparePrec.colorThreshold = tcu::computeFixedPointThreshold(cmpBits);
        comparePrec.filterBits     = tcu::IVec3(4, 4, 0);

        const qpTestResult compareResult =
            compareGenMipmapResult(m_testCtx.getLog(), resultTexture, m_texture->getRefTexture(), comparePrec);

        m_testCtx.setTestResult(compareResult, compareResult == QP_TEST_RESULT_PASS ? "Pass" :
                                               compareResult == QP_TEST_RESULT_QUALITY_WARNING ?
                                                                                      "Low-quality method used" :
                                               compareResult == QP_TEST_RESULT_FAIL ? "Image comparison failed" :
                                                                                      "");
    }

    return STOP;
}

// Texture3DMipmapCase

class Texture3DMipmapCase : public TestCase
{
public:
    Texture3DMipmapCase(Context &context, const char *name, const char *desc, CoordType coordType, uint32_t minFilter,
                        uint32_t wrapS, uint32_t wrapT, uint32_t wrapR, uint32_t format, int width, int height,
                        int depth);
    ~Texture3DMipmapCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture3DMipmapCase(const Texture3DMipmapCase &other);
    Texture3DMipmapCase &operator=(const Texture3DMipmapCase &other);

    CoordType m_coordType;
    uint32_t m_minFilter;
    uint32_t m_wrapS;
    uint32_t m_wrapT;
    uint32_t m_wrapR;
    uint32_t m_internalFormat;
    int m_width;
    int m_height;
    int m_depth;

    glu::Texture3D *m_texture;
    TextureTestUtil::TextureRenderer m_renderer;
};

Texture3DMipmapCase::Texture3DMipmapCase(Context &context, const char *name, const char *desc, CoordType coordType,
                                         uint32_t minFilter, uint32_t wrapS, uint32_t wrapT, uint32_t wrapR,
                                         uint32_t format, int width, int height, int depth)
    : TestCase(context, name, desc)
    , m_coordType(coordType)
    , m_minFilter(minFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_wrapR(wrapR)
    , m_internalFormat(format)
    , m_width(width)
    , m_height(height)
    , m_depth(depth)
    , m_texture(nullptr)
    , m_renderer(context.getRenderContext(), context.getTestContext().getLog(), glu::GLSL_VERSION_300_ES,
                 glu::PRECISION_HIGHP)
{
}

Texture3DMipmapCase::~Texture3DMipmapCase(void)
{
    Texture3DMipmapCase::deinit();
}

void Texture3DMipmapCase::init(void)
{
    const tcu::TextureFormat &texFmt = glu::mapGLInternalFormat(m_internalFormat);
    tcu::TextureFormatInfo fmtInfo   = tcu::getTextureFormatInfo(texFmt);
    const tcu::Vec4 &cScale          = fmtInfo.lookupScale;
    const tcu::Vec4 &cBias           = fmtInfo.lookupBias;
    int numLevels                    = deLog2Floor32(de::max(de::max(m_width, m_height), m_depth)) + 1;

    if (m_coordType == COORDTYPE_PROJECTED && m_context.getRenderTarget().getNumSamples() > 0)
        throw tcu::NotSupportedError("Projected lookup validation not supported in multisample config");

    m_texture = new glu::Texture3D(m_context.getRenderContext(), m_internalFormat, m_width, m_height, m_depth);

    // Fill texture with colored grid.
    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        uint32_t step  = 0xff / (numLevels - 1);
        uint32_t inc   = deClamp32(step * levelNdx, 0x00, 0xff);
        uint32_t dec   = 0xff - inc;
        uint32_t rgb   = (0xff << 16) | (dec << 8) | inc;
        uint32_t color = 0xff000000 | rgb;

        m_texture->getRefTexture().allocLevel(levelNdx);
        tcu::clear(m_texture->getRefTexture().getLevel(levelNdx), tcu::RGBA(color).toVec() * cScale + cBias);
    }

    m_texture->upload();
}

void Texture3DMipmapCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

static void getBasicTexCoord3D(std::vector<float> &dst, int cellNdx)
{
    static const struct
    {
        float sScale;
        float sBias;
        float tScale;
        float tBias;
        float rScale;
        float rBias;
    } s_params[] = {//        sScale    sBias    tScale    tBias    rScale    rBias
                    {0.9f, -0.1f, 0.7f, 0.3f, 0.8f, 0.9f},    {1.2f, -0.1f, 1.1f, 0.3f, 1.0f, 0.9f},
                    {1.5f, 0.7f, 0.9f, -0.3f, 1.1f, 0.1f},    {1.2f, 0.7f, -2.3f, -0.3f, 1.1f, 0.2f},
                    {1.1f, 0.8f, -1.3f, -0.3f, 2.9f, 0.9f},   {3.4f, 0.8f, 4.0f, 0.0f, -3.3f, -1.0f},
                    {-3.4f, -0.1f, -4.0f, 0.0f, -5.1f, 1.0f}, {-4.0f, -0.1f, 3.4f, 0.1f, 5.7f, 0.0f},
                    {-5.6f, 0.0f, 0.5f, 1.2f, 3.9f, 4.0f},    {5.0f, -2.0f, 3.1f, 1.2f, 5.1f, 0.2f},
                    {2.5f, -2.0f, 6.3f, 3.0f, 5.1f, 0.2f},    {-8.3f, 0.0f, 7.1f, 3.0f, 2.0f, 0.2f},
                    {3.8f, 0.0f, 9.7f, 1.0f, 7.0f, 0.7f},     {13.3f, 0.0f, 7.1f, 3.0f, 2.0f, 0.2f},
                    {16.0f, 8.0f, 12.7f, 1.0f, 17.1f, 0.7f},  {15.3f, 0.0f, 20.1f, 3.0f, 33.0f, 3.2f}};

    float sScale = s_params[cellNdx % DE_LENGTH_OF_ARRAY(s_params)].sScale;
    float sBias  = s_params[cellNdx % DE_LENGTH_OF_ARRAY(s_params)].sBias;
    float tScale = s_params[cellNdx % DE_LENGTH_OF_ARRAY(s_params)].tScale;
    float tBias  = s_params[cellNdx % DE_LENGTH_OF_ARRAY(s_params)].tBias;
    float rScale = s_params[cellNdx % DE_LENGTH_OF_ARRAY(s_params)].rScale;
    float rBias  = s_params[cellNdx % DE_LENGTH_OF_ARRAY(s_params)].rBias;

    dst.resize(3 * 4);

    dst[0]  = sBias;
    dst[1]  = tBias;
    dst[2]  = rBias;
    dst[3]  = sBias;
    dst[4]  = tBias + tScale;
    dst[5]  = rBias + rScale * 0.5f;
    dst[6]  = sBias + sScale;
    dst[7]  = tBias;
    dst[8]  = rBias + rScale * 0.5f;
    dst[9]  = sBias + sScale;
    dst[10] = tBias + tScale;
    dst[11] = rBias + rScale;
}

static void getAffineTexCoord3D(std::vector<float> &dst, int cellNdx)
{
    // Use basic coords as base.
    getBasicTexCoord3D(dst, cellNdx);

    // Rotate based on cell index.
    float angleX        = 0.0f + 2.0f * DE_PI * ((float)cellNdx / 16.0f);
    float angleY        = 1.0f + 2.0f * DE_PI * ((float)cellNdx / 32.0f);
    tcu::Mat3 rotMatrix = tcu::rotationMatrixX(angleX) * tcu::rotationMatrixY(angleY);

    Vec3 p0 = rotMatrix * Vec3(dst[0], dst[1], dst[2]);
    Vec3 p1 = rotMatrix * Vec3(dst[3], dst[4], dst[5]);
    Vec3 p2 = rotMatrix * Vec3(dst[6], dst[7], dst[8]);
    Vec3 p3 = rotMatrix * Vec3(dst[9], dst[10], dst[11]);

    dst[0]  = p0.x();
    dst[1]  = p0.y();
    dst[2]  = p0.z();
    dst[3]  = p1.x();
    dst[4]  = p1.y();
    dst[5]  = p1.z();
    dst[6]  = p2.x();
    dst[7]  = p2.y();
    dst[8]  = p2.z();
    dst[9]  = p3.x();
    dst[10] = p3.y();
    dst[11] = p3.z();
}

Texture3DMipmapCase::IterateResult Texture3DMipmapCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    const tcu::Texture3D &refTexture     = m_texture->getRefTexture();
    const tcu::TextureFormat &texFmt     = refTexture.getFormat();
    const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);
    const int texWidth                   = refTexture.getWidth();
    const int texHeight                  = refTexture.getHeight();
    const uint32_t magFilter             = GL_NEAREST;

    const tcu::RenderTarget &renderTarget = m_context.getRenderContext().getRenderTarget();
    const RandomViewport viewport(renderTarget, texWidth * 4, texHeight * 4, deStringHash(getName()));

    const bool isProjected = m_coordType == COORDTYPE_PROJECTED;
    const bool useLodBias  = m_coordType == COORDTYPE_BASIC_BIAS;

    // Viewport is divided into 4x4 grid.
    const int gridWidth  = 4;
    const int gridHeight = 4;
    const int cellWidth  = viewport.width / gridWidth;
    const int cellHeight = viewport.height / gridHeight;

    ReferenceParams sampleParams(TEXTURETYPE_3D);

    tcu::Surface renderedFrame(viewport.width, viewport.height);
    vector<float> texCoord;

    // Sampling parameters.
    sampleParams.sampler     = glu::mapGLSampler(m_wrapS, m_wrapT, m_wrapR, m_minFilter, magFilter);
    sampleParams.samplerType = getSamplerType(texFmt);
    sampleParams.colorBias   = fmtInfo.lookupBias;
    sampleParams.colorScale  = fmtInfo.lookupScale;
    sampleParams.flags = (isProjected ? ReferenceParams::PROJECTED : 0) | (useLodBias ? ReferenceParams::USE_BIAS : 0);

    // Bind texture and setup sampler parameters.
    gl.bindTexture(GL_TEXTURE_3D, m_texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, m_wrapS);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, m_wrapT);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, m_wrapR);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, magFilter);

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
            const int curX    = cellWidth * gridX;
            const int curY    = cellHeight * gridY;
            const int curW    = gridX + 1 == gridWidth ? (viewport.width - curX) : cellWidth;
            const int curH    = gridY + 1 == gridHeight ? (viewport.height - curY) : cellHeight;
            const int cellNdx = gridY * gridWidth + gridX;

            // Compute texcoord.
            switch (m_coordType)
            {
            case COORDTYPE_BASIC_BIAS: // Fall-through.
            case COORDTYPE_PROJECTED:
            case COORDTYPE_BASIC:
                getBasicTexCoord3D(texCoord, cellNdx);
                break;
            case COORDTYPE_AFFINE:
                getAffineTexCoord3D(texCoord, cellNdx);
                break;
            default:
                DE_ASSERT(false);
            }

            // Set projection.
            if (isProjected)
                sampleParams.w = s_projections[cellNdx % DE_LENGTH_OF_ARRAY(s_projections)];

            // Set LOD bias.
            if (useLodBias)
                sampleParams.bias = s_bias[cellNdx % DE_LENGTH_OF_ARRAY(s_bias)];

            // Render with GL.
            gl.viewport(viewport.x + curX, viewport.y + curY, curW, curH);
            m_renderer.renderQuad(0, &texCoord[0], sampleParams);
        }
    }

    // Read result.
    glu::readPixels(m_context.getRenderContext(), viewport.x, viewport.y, renderedFrame.getAccess());

    // Compare and log
    {
        const tcu::PixelFormat &pixelFormat = m_context.getRenderTarget().getPixelFormat();
        const bool isTrilinear = m_minFilter == GL_NEAREST_MIPMAP_LINEAR || m_minFilter == GL_LINEAR_MIPMAP_LINEAR;
        tcu::Surface referenceFrame(viewport.width, viewport.height);
        tcu::Surface errorMask(viewport.width, viewport.height);
        tcu::LookupPrecision lookupPrec;
        tcu::LodPrecision lodPrec;
        int numFailedPixels = 0;

        lookupPrec.coordBits = tcu::IVec3(20, 20, 20);
        lookupPrec.uvwBits   = tcu::IVec3(16, 16, 16); // Doesn't really matter since pixels are unicolored.
        lookupPrec.colorThreshold =
            tcu::computeFixedPointThreshold(max(getBitsVec(pixelFormat) - (isTrilinear ? 2 : 1), tcu::IVec4(0)));
        lookupPrec.colorMask = getCompareMask(pixelFormat);
        lodPrec.derivateBits = 10;
        lodPrec.lodBits      = isProjected ? 6 : 8;

        for (int gridY = 0; gridY < gridHeight; gridY++)
        {
            for (int gridX = 0; gridX < gridWidth; gridX++)
            {
                const int curX    = cellWidth * gridX;
                const int curY    = cellHeight * gridY;
                const int curW    = gridX + 1 == gridWidth ? (viewport.width - curX) : cellWidth;
                const int curH    = gridY + 1 == gridHeight ? (viewport.height - curY) : cellHeight;
                const int cellNdx = gridY * gridWidth + gridX;

                switch (m_coordType)
                {
                case COORDTYPE_BASIC_BIAS: // Fall-through.
                case COORDTYPE_PROJECTED:
                case COORDTYPE_BASIC:
                    getBasicTexCoord3D(texCoord, cellNdx);
                    break;
                case COORDTYPE_AFFINE:
                    getAffineTexCoord3D(texCoord, cellNdx);
                    break;
                default:
                    DE_ASSERT(false);
                }

                if (isProjected)
                    sampleParams.w = s_projections[cellNdx % DE_LENGTH_OF_ARRAY(s_projections)];

                if (useLodBias)
                    sampleParams.bias = s_bias[cellNdx % DE_LENGTH_OF_ARRAY(s_bias)];

                // Render ideal result
                sampleTexture(tcu::SurfaceAccess(referenceFrame, pixelFormat, curX, curY, curW, curH), refTexture,
                              &texCoord[0], sampleParams);

                // Compare this cell
                numFailedPixels += computeTextureLookupDiff(
                    tcu::getSubregion(renderedFrame.getAccess(), curX, curY, curW, curH),
                    tcu::getSubregion(referenceFrame.getAccess(), curX, curY, curW, curH),
                    tcu::getSubregion(errorMask.getAccess(), curX, curY, curW, curH), m_texture->getRefTexture(),
                    &texCoord[0], sampleParams, lookupPrec, lodPrec, m_testCtx.getWatchDog());
            }
        }

        if (numFailedPixels > 0)
            m_testCtx.getLog() << TestLog::Message << "ERROR: Image verification failed, found " << numFailedPixels
                               << " invalid pixels!" << TestLog::EndMessage;

        m_testCtx.getLog() << TestLog::ImageSet("Result", "Verification result")
                           << TestLog::Image("Rendered", "Rendered image", renderedFrame);

        if (numFailedPixels > 0)
        {
            m_testCtx.getLog() << TestLog::Image("Reference", "Ideal reference", referenceFrame)
                               << TestLog::Image("ErrorMask", "Error mask", errorMask);
        }

        m_testCtx.getLog() << TestLog::EndImageSet;

        {
            const bool isOk = numFailedPixels == 0;
            m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                    isOk ? "Pass" : "Image verification failed");
        }
    }

    return STOP;
}

// Texture2DLodControlCase + test cases

class Texture2DLodControlCase : public TestCase
{
public:
    Texture2DLodControlCase(Context &context, const char *name, const char *desc, uint32_t minFilter);
    ~Texture2DLodControlCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

protected:
    virtual void setTextureParams(int cellNdx)                            = 0;
    virtual void getReferenceParams(ReferenceParams &params, int cellNdx) = 0;

    const int m_texWidth;
    const int m_texHeight;

private:
    Texture2DLodControlCase(const Texture2DLodControlCase &other);
    Texture2DLodControlCase &operator=(const Texture2DLodControlCase &other);

    uint32_t m_minFilter;

    glu::Texture2D *m_texture;
    TextureTestUtil::TextureRenderer m_renderer;
};

Texture2DLodControlCase::Texture2DLodControlCase(Context &context, const char *name, const char *desc,
                                                 uint32_t minFilter)
    : TestCase(context, name, desc)
    , m_texWidth(64)
    , m_texHeight(64)
    , m_minFilter(minFilter)
    , m_texture(nullptr)
    , m_renderer(context.getRenderContext(), context.getTestContext().getLog(), glu::GLSL_VERSION_300_ES,
                 glu::PRECISION_HIGHP)
{
}

Texture2DLodControlCase::~Texture2DLodControlCase(void)
{
    Texture2DLodControlCase::deinit();
}

void Texture2DLodControlCase::init(void)
{
    const uint32_t format = GL_RGBA8;
    int numLevels         = deLog2Floor32(de::max(m_texWidth, m_texHeight)) + 1;

    m_texture = new glu::Texture2D(m_context.getRenderContext(), format, m_texWidth, m_texHeight);

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

void Texture2DLodControlCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

Texture2DLodControlCase::IterateResult Texture2DLodControlCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    const uint32_t wrapS     = GL_REPEAT;
    const uint32_t wrapT     = GL_REPEAT;
    const uint32_t magFilter = GL_NEAREST;

    const tcu::Texture2D &refTexture = m_texture->getRefTexture();
    const int texWidth               = refTexture.getWidth();
    const int texHeight              = refTexture.getHeight();

    const tcu::RenderTarget &renderTarget = m_context.getRenderContext().getRenderTarget();
    const RandomViewport viewport(renderTarget, texWidth * 4, texHeight * 4, deStringHash(getName()));

    ReferenceParams sampleParams(TEXTURETYPE_2D, glu::mapGLSampler(wrapS, wrapT, m_minFilter, magFilter));
    vector<float> texCoord;
    tcu::Surface renderedFrame(viewport.width, viewport.height);

    // Viewport is divided into 4x4 grid.
    const int gridWidth  = 4;
    const int gridHeight = 4;
    const int cellWidth  = viewport.width / gridWidth;
    const int cellHeight = viewport.height / gridHeight;

    // Upload texture data.
    m_texture->upload();

    // Bind gradient texture and setup sampler parameters.
    gl.bindTexture(GL_TEXTURE_2D, m_texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

    GLU_EXPECT_NO_ERROR(gl.getError(), "After texture setup");

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
            getBasicTexCoord2D(texCoord, cellNdx);

            // Render with GL.
            setTextureParams(cellNdx);
            gl.viewport(viewport.x + curX, viewport.y + curY, curW, curH);
            m_renderer.renderQuad(0, &texCoord[0], sampleParams);
        }
    }

    glu::readPixels(m_context.getRenderContext(), viewport.x, viewport.y, renderedFrame.getAccess());
    GLU_EXPECT_NO_ERROR(gl.getError(), "Read pixels");

    // Compare and log.
    {
        const tcu::PixelFormat &pixelFormat = m_context.getRenderTarget().getPixelFormat();
        const bool isTrilinear = m_minFilter == GL_NEAREST_MIPMAP_LINEAR || m_minFilter == GL_LINEAR_MIPMAP_LINEAR;
        tcu::Surface referenceFrame(viewport.width, viewport.height);
        tcu::Surface errorMask(viewport.width, viewport.height);
        tcu::LookupPrecision lookupPrec;
        tcu::LodPrecision lodPrec;
        int numFailedPixels = 0;

        lookupPrec.coordBits = tcu::IVec3(20, 20, 0);
        lookupPrec.uvwBits   = tcu::IVec3(16, 16, 0); // Doesn't really matter since pixels are unicolored.
        lookupPrec.colorThreshold =
            tcu::computeFixedPointThreshold(max(getBitsVec(pixelFormat) - (isTrilinear ? 2 : 1), tcu::IVec4(0)));
        lookupPrec.colorMask = getCompareMask(pixelFormat);
        lodPrec.derivateBits = 10;
        lodPrec.lodBits      = 8;

        for (int gridY = 0; gridY < gridHeight; gridY++)
        {
            for (int gridX = 0; gridX < gridWidth; gridX++)
            {
                const int curX    = cellWidth * gridX;
                const int curY    = cellHeight * gridY;
                const int curW    = gridX + 1 == gridWidth ? (viewport.width - curX) : cellWidth;
                const int curH    = gridY + 1 == gridHeight ? (viewport.height - curY) : cellHeight;
                const int cellNdx = gridY * gridWidth + gridX;

                getBasicTexCoord2D(texCoord, cellNdx);
                getReferenceParams(sampleParams, cellNdx);

                // Render ideal result
                sampleTexture(tcu::SurfaceAccess(referenceFrame, pixelFormat, curX, curY, curW, curH), refTexture,
                              &texCoord[0], sampleParams);

                // Compare this cell
                numFailedPixels += computeTextureLookupDiff(
                    tcu::getSubregion(renderedFrame.getAccess(), curX, curY, curW, curH),
                    tcu::getSubregion(referenceFrame.getAccess(), curX, curY, curW, curH),
                    tcu::getSubregion(errorMask.getAccess(), curX, curY, curW, curH), m_texture->getRefTexture(),
                    &texCoord[0], sampleParams, lookupPrec, lodPrec, m_testCtx.getWatchDog());
            }
        }

        if (numFailedPixels > 0)
            m_testCtx.getLog() << TestLog::Message << "ERROR: Image verification failed, found " << numFailedPixels
                               << " invalid pixels!" << TestLog::EndMessage;

        m_testCtx.getLog() << TestLog::ImageSet("Result", "Verification result")
                           << TestLog::Image("Rendered", "Rendered image", renderedFrame);

        if (numFailedPixels > 0)
        {
            m_testCtx.getLog() << TestLog::Image("Reference", "Ideal reference", referenceFrame)
                               << TestLog::Image("ErrorMask", "Error mask", errorMask);
        }

        m_testCtx.getLog() << TestLog::EndImageSet;

        {
            const bool isOk = numFailedPixels == 0;
            m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                    isOk ? "Pass" : "Image verification failed");
        }
    }

    return STOP;
}

class Texture2DMinLodCase : public Texture2DLodControlCase
{
public:
    Texture2DMinLodCase(Context &context, const char *name, const char *desc, uint32_t minFilter)
        : Texture2DLodControlCase(context, name, desc, minFilter)
    {
    }

protected:
    void setTextureParams(int cellNdx)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        gl.texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, getMinLodForCell(cellNdx));
    }

    void getReferenceParams(ReferenceParams &params, int cellNdx)
    {
        params.minLod = getMinLodForCell(cellNdx);
    }
};

class Texture2DMaxLodCase : public Texture2DLodControlCase
{
public:
    Texture2DMaxLodCase(Context &context, const char *name, const char *desc, uint32_t minFilter)
        : Texture2DLodControlCase(context, name, desc, minFilter)
    {
    }

protected:
    void setTextureParams(int cellNdx)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        gl.texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, getMaxLodForCell(cellNdx));
    }

    void getReferenceParams(ReferenceParams &params, int cellNdx)
    {
        params.maxLod = getMaxLodForCell(cellNdx);
    }
};

class Texture2DBaseLevelCase : public Texture2DLodControlCase
{
public:
    Texture2DBaseLevelCase(Context &context, const char *name, const char *desc, uint32_t minFilter)
        : Texture2DLodControlCase(context, name, desc, minFilter)
    {
    }

protected:
    int getBaseLevel(int cellNdx) const
    {
        const int numLevels = deLog2Floor32(de::max(m_texWidth, m_texHeight)) + 1;
        const int baseLevel = (deInt32Hash(cellNdx) ^ deStringHash(getName()) ^ 0xac2f274a) % numLevels;

        return baseLevel;
    }

    void setTextureParams(int cellNdx)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, getBaseLevel(cellNdx));
    }

    void getReferenceParams(ReferenceParams &params, int cellNdx)
    {
        params.baseLevel = getBaseLevel(cellNdx);
    }
};

class Texture2DMaxLevelCase : public Texture2DLodControlCase
{
public:
    Texture2DMaxLevelCase(Context &context, const char *name, const char *desc, uint32_t minFilter)
        : Texture2DLodControlCase(context, name, desc, minFilter)
    {
    }

protected:
    int getMaxLevel(int cellNdx) const
    {
        const int numLevels = deLog2Floor32(de::max(m_texWidth, m_texHeight)) + 1;
        const int maxLevel  = (deInt32Hash(cellNdx) ^ deStringHash(getName()) ^ 0x82cfa4e) % numLevels;

        return maxLevel;
    }

    void setTextureParams(int cellNdx)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, getMaxLevel(cellNdx));
    }

    void getReferenceParams(ReferenceParams &params, int cellNdx)
    {
        params.maxLevel = getMaxLevel(cellNdx);
    }
};

// TextureCubeLodControlCase + test cases

class TextureCubeLodControlCase : public TestCase
{
public:
    TextureCubeLodControlCase(Context &context, const char *name, const char *desc, uint32_t minFilter);
    ~TextureCubeLodControlCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

protected:
    virtual void setTextureParams(int cellNdx)                            = 0;
    virtual void getReferenceParams(ReferenceParams &params, int cellNdx) = 0;

    const int m_texSize;

private:
    TextureCubeLodControlCase(const TextureCubeLodControlCase &other);
    TextureCubeLodControlCase &operator=(const TextureCubeLodControlCase &other);

    uint32_t m_minFilter;

    glu::TextureCube *m_texture;
    TextureTestUtil::TextureRenderer m_renderer;
};

TextureCubeLodControlCase::TextureCubeLodControlCase(Context &context, const char *name, const char *desc,
                                                     uint32_t minFilter)
    : TestCase(context, name, desc)
    , m_texSize(64)
    , m_minFilter(minFilter)
    , m_texture(nullptr)
    , m_renderer(context.getRenderContext(), context.getTestContext().getLog(), glu::GLSL_VERSION_300_ES,
                 glu::PRECISION_HIGHP)
{
}

TextureCubeLodControlCase::~TextureCubeLodControlCase(void)
{
    deinit();
}

void TextureCubeLodControlCase::init(void)
{
    const uint32_t format = GL_RGBA8;
    const int numLevels   = deLog2Floor32(m_texSize) + 1;

    m_texture = new glu::TextureCube(m_context.getRenderContext(), format, m_texSize);

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

void TextureCubeLodControlCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

TextureCubeLodControlCase::IterateResult TextureCubeLodControlCase::iterate(void)
{
    const uint32_t wrapS     = GL_CLAMP_TO_EDGE;
    const uint32_t wrapT     = GL_CLAMP_TO_EDGE;
    const uint32_t magFilter = GL_NEAREST;

    const int texWidth  = m_texture->getRefTexture().getSize();
    const int texHeight = m_texture->getRefTexture().getSize();

    const int defViewportWidth  = texWidth * 2;
    const int defViewportHeight = texHeight * 2;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const RandomViewport viewport(m_context.getRenderTarget(), defViewportWidth, defViewportHeight,
                                  deStringHash(getName()));

    vector<float> texCoord;

    tcu::Surface renderedFrame(viewport.width, viewport.height);

    // Upload texture data.
    m_texture->upload();

    // Bind gradient texture and setup sampler parameters.
    gl.bindTexture(GL_TEXTURE_CUBE_MAP, m_texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, wrapS);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, wrapT);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, magFilter);

    GLU_EXPECT_NO_ERROR(gl.getError(), "After texture setup");

    // Compute grid.
    vector<tcu::IVec4> gridLayout;
    computeGridLayout(gridLayout, viewport.width, viewport.height);

    for (int cellNdx = 0; cellNdx < (int)gridLayout.size(); cellNdx++)
    {
        const int curX               = gridLayout[cellNdx].x();
        const int curY               = gridLayout[cellNdx].y();
        const int curW               = gridLayout[cellNdx].z();
        const int curH               = gridLayout[cellNdx].w();
        const tcu::CubeFace cubeFace = (tcu::CubeFace)(cellNdx % tcu::CUBEFACE_LAST);
        RenderParams params(TEXTURETYPE_CUBE);

        computeQuadTexCoordCube(texCoord, cubeFace);

        setTextureParams(cellNdx);

        // Render with GL.
        gl.viewport(viewport.x + curX, viewport.y + curY, curW, curH);
        m_renderer.renderQuad(0, &texCoord[0], params);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Draw");
    }

    // Read result.
    glu::readPixels(m_context.getRenderContext(), viewport.x, viewport.y, renderedFrame.getAccess());
    GLU_EXPECT_NO_ERROR(gl.getError(), "Read pixels");

    // Render reference and compare
    {
        tcu::Surface referenceFrame(viewport.width, viewport.height);
        tcu::Surface errorMask(viewport.width, viewport.height);
        int numFailedPixels = 0;
        ReferenceParams params(TEXTURETYPE_CUBE);
        tcu::LookupPrecision lookupPrec;
        tcu::LodPrecision lodPrec;

        // Params for rendering reference
        params.sampler                 = glu::mapGLSampler(wrapS, wrapT, m_minFilter, magFilter);
        params.sampler.seamlessCubeMap = true;
        params.lodMode                 = LODMODE_EXACT;

        // Comparison parameters
        lookupPrec.colorMask      = getCompareMask(m_context.getRenderTarget().getPixelFormat());
        lookupPrec.colorThreshold = tcu::computeFixedPointThreshold(
            max(getBitsVec(m_context.getRenderTarget().getPixelFormat()) - 2, IVec4(0)));
        lookupPrec.coordBits = tcu::IVec3(10);
        lookupPrec.uvwBits   = tcu::IVec3(5, 5, 0);
        lodPrec.derivateBits = 10;
        lodPrec.lodBits      = 6;

        for (int cellNdx = 0; cellNdx < (int)gridLayout.size(); cellNdx++)
        {
            const int curX               = gridLayout[cellNdx].x();
            const int curY               = gridLayout[cellNdx].y();
            const int curW               = gridLayout[cellNdx].z();
            const int curH               = gridLayout[cellNdx].w();
            const tcu::CubeFace cubeFace = (tcu::CubeFace)(cellNdx % tcu::CUBEFACE_LAST);

            computeQuadTexCoordCube(texCoord, cubeFace);
            getReferenceParams(params, cellNdx);

            // Render ideal reference.
            {
                tcu::SurfaceAccess idealDst(referenceFrame, m_context.getRenderTarget().getPixelFormat(), curX, curY,
                                            curW, curH);
                sampleTexture(idealDst, m_texture->getRefTexture(), &texCoord[0], params);
            }

            // Compare this cell
            numFailedPixels += computeTextureLookupDiff(
                tcu::getSubregion(renderedFrame.getAccess(), curX, curY, curW, curH),
                tcu::getSubregion(referenceFrame.getAccess(), curX, curY, curW, curH),
                tcu::getSubregion(errorMask.getAccess(), curX, curY, curW, curH), m_texture->getRefTexture(),
                &texCoord[0], params, lookupPrec, lodPrec, m_testCtx.getWatchDog());
        }

        if (numFailedPixels > 0)
            m_testCtx.getLog() << TestLog::Message << "ERROR: Image verification failed, found " << numFailedPixels
                               << " invalid pixels!" << TestLog::EndMessage;

        m_testCtx.getLog() << TestLog::ImageSet("Result", "Verification result")
                           << TestLog::Image("Rendered", "Rendered image", renderedFrame);

        if (numFailedPixels > 0)
        {
            m_testCtx.getLog() << TestLog::Image("Reference", "Ideal reference", referenceFrame)
                               << TestLog::Image("ErrorMask", "Error mask", errorMask);
        }

        m_testCtx.getLog() << TestLog::EndImageSet;

        {
            const bool isOk = numFailedPixels == 0;
            m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                    isOk ? "Pass" : "Image verification failed");
        }
    }

    return STOP;
}

class TextureCubeMinLodCase : public TextureCubeLodControlCase
{
public:
    TextureCubeMinLodCase(Context &context, const char *name, const char *desc, uint32_t minFilter)
        : TextureCubeLodControlCase(context, name, desc, minFilter)
    {
    }

protected:
    void setTextureParams(int cellNdx)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        gl.texParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_LOD, getMinLodForCell(cellNdx));
    }

    void getReferenceParams(ReferenceParams &params, int cellNdx)
    {
        params.minLod = getMinLodForCell(cellNdx);
    }
};

class TextureCubeMaxLodCase : public TextureCubeLodControlCase
{
public:
    TextureCubeMaxLodCase(Context &context, const char *name, const char *desc, uint32_t minFilter)
        : TextureCubeLodControlCase(context, name, desc, minFilter)
    {
    }

protected:
    void setTextureParams(int cellNdx)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        gl.texParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LOD, getMaxLodForCell(cellNdx));
    }

    void getReferenceParams(ReferenceParams &params, int cellNdx)
    {
        params.maxLod = getMaxLodForCell(cellNdx);
    }
};

class TextureCubeBaseLevelCase : public TextureCubeLodControlCase
{
public:
    TextureCubeBaseLevelCase(Context &context, const char *name, const char *desc, uint32_t minFilter)
        : TextureCubeLodControlCase(context, name, desc, minFilter)
    {
    }

protected:
    int getBaseLevel(int cellNdx) const
    {
        const int numLevels = deLog2Floor32(m_texSize) + 1;
        const int baseLevel = (deInt32Hash(cellNdx) ^ deStringHash(getName()) ^ 0x23fae13) % numLevels;

        return baseLevel;
    }

    void setTextureParams(int cellNdx)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, getBaseLevel(cellNdx));
    }

    void getReferenceParams(ReferenceParams &params, int cellNdx)
    {
        params.baseLevel = getBaseLevel(cellNdx);
    }
};

class TextureCubeMaxLevelCase : public TextureCubeLodControlCase
{
public:
    TextureCubeMaxLevelCase(Context &context, const char *name, const char *desc, uint32_t minFilter)
        : TextureCubeLodControlCase(context, name, desc, minFilter)
    {
    }

protected:
    int getMaxLevel(int cellNdx) const
    {
        const int numLevels = deLog2Floor32(m_texSize) + 1;
        const int maxLevel  = (deInt32Hash(cellNdx) ^ deStringHash(getName()) ^ 0x974e21) % numLevels;

        return maxLevel;
    }

    void setTextureParams(int cellNdx)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, getMaxLevel(cellNdx));
    }

    void getReferenceParams(ReferenceParams &params, int cellNdx)
    {
        params.maxLevel = getMaxLevel(cellNdx);
    }
};

// Texture3DLodControlCase + test cases

class Texture3DLodControlCase : public TestCase
{
public:
    Texture3DLodControlCase(Context &context, const char *name, const char *desc, uint32_t minFilter);
    ~Texture3DLodControlCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

protected:
    virtual void setTextureParams(int cellNdx)                            = 0;
    virtual void getReferenceParams(ReferenceParams &params, int cellNdx) = 0;

    const int m_texWidth;
    const int m_texHeight;
    const int m_texDepth;

private:
    Texture3DLodControlCase(const Texture3DLodControlCase &other);
    Texture3DLodControlCase &operator=(const Texture3DLodControlCase &other);

    uint32_t m_minFilter;

    glu::Texture3D *m_texture;
    TextureTestUtil::TextureRenderer m_renderer;
};

Texture3DLodControlCase::Texture3DLodControlCase(Context &context, const char *name, const char *desc,
                                                 uint32_t minFilter)
    : TestCase(context, name, desc)
    , m_texWidth(32)
    , m_texHeight(32)
    , m_texDepth(32)
    , m_minFilter(minFilter)
    , m_texture(nullptr)
    , m_renderer(context.getRenderContext(), context.getTestContext().getLog(), glu::GLSL_VERSION_300_ES,
                 glu::PRECISION_HIGHP)
{
}

Texture3DLodControlCase::~Texture3DLodControlCase(void)
{
    Texture3DLodControlCase::deinit();
}

void Texture3DLodControlCase::init(void)
{
    const uint32_t format            = GL_RGBA8;
    const tcu::TextureFormat &texFmt = glu::mapGLInternalFormat(format);
    tcu::TextureFormatInfo fmtInfo   = tcu::getTextureFormatInfo(texFmt);
    const tcu::Vec4 &cScale          = fmtInfo.lookupScale;
    const tcu::Vec4 &cBias           = fmtInfo.lookupBias;
    int numLevels                    = deLog2Floor32(de::max(de::max(m_texWidth, m_texHeight), m_texDepth)) + 1;

    m_texture = new glu::Texture3D(m_context.getRenderContext(), format, m_texWidth, m_texHeight, m_texDepth);

    // Fill texture with colored grid.
    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        uint32_t step  = 0xff / (numLevels - 1);
        uint32_t inc   = deClamp32(step * levelNdx, 0x00, 0xff);
        uint32_t dec   = 0xff - inc;
        uint32_t rgb   = (inc << 16) | (dec << 8) | 0xff;
        uint32_t color = 0xff000000 | rgb;

        m_texture->getRefTexture().allocLevel(levelNdx);
        tcu::clear(m_texture->getRefTexture().getLevel(levelNdx), tcu::RGBA(color).toVec() * cScale + cBias);
    }

    m_texture->upload();
}

void Texture3DLodControlCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

Texture3DLodControlCase::IterateResult Texture3DLodControlCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    const uint32_t wrapS                 = GL_CLAMP_TO_EDGE;
    const uint32_t wrapT                 = GL_CLAMP_TO_EDGE;
    const uint32_t wrapR                 = GL_CLAMP_TO_EDGE;
    const uint32_t magFilter             = GL_NEAREST;
    const tcu::Texture3D &refTexture     = m_texture->getRefTexture();
    const tcu::TextureFormat &texFmt     = refTexture.getFormat();
    const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);
    const int texWidth                   = refTexture.getWidth();
    const int texHeight                  = refTexture.getHeight();

    const tcu::RenderTarget &renderTarget = m_context.getRenderContext().getRenderTarget();
    const RandomViewport viewport(renderTarget, texWidth * 4, texHeight * 4, deStringHash(getName()));

    // Viewport is divided into 4x4 grid.
    const int gridWidth  = 4;
    const int gridHeight = 4;
    const int cellWidth  = viewport.width / gridWidth;
    const int cellHeight = viewport.height / gridHeight;

    tcu::Surface renderedFrame(viewport.width, viewport.height);
    vector<float> texCoord;
    ReferenceParams sampleParams(TEXTURETYPE_3D);

    // Sampling parameters.
    sampleParams.sampler     = glu::mapGLSampler(wrapS, wrapT, wrapR, m_minFilter, magFilter);
    sampleParams.samplerType = getSamplerType(texFmt);
    sampleParams.colorBias   = fmtInfo.lookupBias;
    sampleParams.colorScale  = fmtInfo.lookupScale;

    // Bind texture and setup sampler parameters.
    gl.bindTexture(GL_TEXTURE_3D, m_texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, wrapS);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, wrapT);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, wrapR);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, magFilter);

    GLU_EXPECT_NO_ERROR(gl.getError(), "After texture setup");

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
            getBasicTexCoord3D(texCoord, cellNdx);

            setTextureParams(cellNdx);

            // Render with GL.
            gl.viewport(viewport.x + curX, viewport.y + curY, curW, curH);
            m_renderer.renderQuad(0, &texCoord[0], sampleParams);
        }
    }

    // Read result.
    glu::readPixels(m_context.getRenderContext(), viewport.x, viewport.y, renderedFrame.getAccess());

    // Compare and log
    {
        const tcu::PixelFormat &pixelFormat = m_context.getRenderTarget().getPixelFormat();
        const bool isTrilinear = m_minFilter == GL_NEAREST_MIPMAP_LINEAR || m_minFilter == GL_LINEAR_MIPMAP_LINEAR;
        tcu::Surface referenceFrame(viewport.width, viewport.height);
        tcu::Surface errorMask(viewport.width, viewport.height);
        tcu::LookupPrecision lookupPrec;
        tcu::LodPrecision lodPrec;
        int numFailedPixels = 0;

        lookupPrec.coordBits = tcu::IVec3(20, 20, 20);
        lookupPrec.uvwBits   = tcu::IVec3(16, 16, 16); // Doesn't really matter since pixels are unicolored.
        lookupPrec.colorThreshold =
            tcu::computeFixedPointThreshold(max(getBitsVec(pixelFormat) - (isTrilinear ? 2 : 1), tcu::IVec4(0)));
        lookupPrec.colorMask = getCompareMask(pixelFormat);
        lodPrec.derivateBits = 10;
        lodPrec.lodBits      = 8;

        for (int gridY = 0; gridY < gridHeight; gridY++)
        {
            for (int gridX = 0; gridX < gridWidth; gridX++)
            {
                const int curX    = cellWidth * gridX;
                const int curY    = cellHeight * gridY;
                const int curW    = gridX + 1 == gridWidth ? (viewport.width - curX) : cellWidth;
                const int curH    = gridY + 1 == gridHeight ? (viewport.height - curY) : cellHeight;
                const int cellNdx = gridY * gridWidth + gridX;

                getBasicTexCoord3D(texCoord, cellNdx);
                getReferenceParams(sampleParams, cellNdx);

                // Render ideal result
                sampleTexture(tcu::SurfaceAccess(referenceFrame, pixelFormat, curX, curY, curW, curH), refTexture,
                              &texCoord[0], sampleParams);

                // Compare this cell
                numFailedPixels += computeTextureLookupDiff(
                    tcu::getSubregion(renderedFrame.getAccess(), curX, curY, curW, curH),
                    tcu::getSubregion(referenceFrame.getAccess(), curX, curY, curW, curH),
                    tcu::getSubregion(errorMask.getAccess(), curX, curY, curW, curH), m_texture->getRefTexture(),
                    &texCoord[0], sampleParams, lookupPrec, lodPrec, m_testCtx.getWatchDog());
            }
        }

        if (numFailedPixels > 0)
            m_testCtx.getLog() << TestLog::Message << "ERROR: Image verification failed, found " << numFailedPixels
                               << " invalid pixels!" << TestLog::EndMessage;

        m_testCtx.getLog() << TestLog::ImageSet("Result", "Verification result")
                           << TestLog::Image("Rendered", "Rendered image", renderedFrame);

        if (numFailedPixels > 0)
        {
            m_testCtx.getLog() << TestLog::Image("Reference", "Ideal reference", referenceFrame)
                               << TestLog::Image("ErrorMask", "Error mask", errorMask);
        }

        m_testCtx.getLog() << TestLog::EndImageSet;

        {
            const bool isOk = numFailedPixels == 0;
            m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                    isOk ? "Pass" : "Image verification failed");
        }
    }

    return STOP;
}

class Texture3DMinLodCase : public Texture3DLodControlCase
{
public:
    Texture3DMinLodCase(Context &context, const char *name, const char *desc, uint32_t minFilter)
        : Texture3DLodControlCase(context, name, desc, minFilter)
    {
    }

protected:
    void setTextureParams(int cellNdx)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        gl.texParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_LOD, getMinLodForCell(cellNdx));
    }

    void getReferenceParams(ReferenceParams &params, int cellNdx)
    {
        params.minLod = getMinLodForCell(cellNdx);
    }
};

class Texture3DMaxLodCase : public Texture3DLodControlCase
{
public:
    Texture3DMaxLodCase(Context &context, const char *name, const char *desc, uint32_t minFilter)
        : Texture3DLodControlCase(context, name, desc, minFilter)
    {
    }

protected:
    void setTextureParams(int cellNdx)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        gl.texParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAX_LOD, getMaxLodForCell(cellNdx));
    }

    void getReferenceParams(ReferenceParams &params, int cellNdx)
    {
        params.maxLod = getMaxLodForCell(cellNdx);
    }
};

class Texture3DBaseLevelCase : public Texture3DLodControlCase
{
public:
    Texture3DBaseLevelCase(Context &context, const char *name, const char *desc, uint32_t minFilter)
        : Texture3DLodControlCase(context, name, desc, minFilter)
    {
    }

protected:
    int getBaseLevel(int cellNdx) const
    {
        const int numLevels = deLog2Floor32(de::max(m_texWidth, de::max(m_texHeight, m_texDepth))) + 1;
        const int baseLevel = (deInt32Hash(cellNdx) ^ deStringHash(getName()) ^ 0x7347e9) % numLevels;

        return baseLevel;
    }

    void setTextureParams(int cellNdx)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, getBaseLevel(cellNdx));
    }

    void getReferenceParams(ReferenceParams &params, int cellNdx)
    {
        params.baseLevel = getBaseLevel(cellNdx);
    }
};

class Texture3DMaxLevelCase : public Texture3DLodControlCase
{
public:
    Texture3DMaxLevelCase(Context &context, const char *name, const char *desc, uint32_t minFilter)
        : Texture3DLodControlCase(context, name, desc, minFilter)
    {
    }

protected:
    int getMaxLevel(int cellNdx) const
    {
        const int numLevels = deLog2Floor32(de::max(m_texWidth, de::max(m_texHeight, m_texDepth))) + 1;
        const int maxLevel  = (deInt32Hash(cellNdx) ^ deStringHash(getName()) ^ 0x9111e7) % numLevels;

        return maxLevel;
    }

    void setTextureParams(int cellNdx)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, getMaxLevel(cellNdx));
    }

    void getReferenceParams(ReferenceParams &params, int cellNdx)
    {
        params.maxLevel = getMaxLevel(cellNdx);
    }
};

TextureMipmapTests::TextureMipmapTests(Context &context) : TestCaseGroup(context, "mipmap", "Mipmapping tests")
{
}

TextureMipmapTests::~TextureMipmapTests(void)
{
}

void TextureMipmapTests::init(void)
{
    tcu::TestCaseGroup *group2D   = new tcu::TestCaseGroup(m_testCtx, "2d", "2D Texture Mipmapping");
    tcu::TestCaseGroup *groupCube = new tcu::TestCaseGroup(m_testCtx, "cube", "Cube Map Mipmapping");
    tcu::TestCaseGroup *group3D   = new tcu::TestCaseGroup(m_testCtx, "3d", "3D Texture Mipmapping");
    addChild(group2D);
    addChild(groupCube);
    addChild(group3D);

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

    static const struct
    {
        const char *name;
        uint32_t format;
        uint32_t dataType;
    } formats[] = {{"a8", GL_ALPHA, GL_UNSIGNED_BYTE},
                   {"l8", GL_LUMINANCE, GL_UNSIGNED_BYTE},
                   {"la88", GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE},
                   {"rgb565", GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
                   {"rgb888", GL_RGB, GL_UNSIGNED_BYTE},
                   {"rgba4444", GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},
                   {"rgba5551", GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1},
                   {"rgba8888", GL_RGBA, GL_UNSIGNED_BYTE}};

    static const struct
    {
        const char *name;
        uint32_t hint;
    } genHints[] = {{"fastest", GL_FASTEST}, {"nicest", GL_NICEST}};

    static const struct
    {
        const char *name;
        int width;
        int height;
    } tex2DSizes[] = {{nullptr, 64, 64}, // Default.
                      {"npot", 63, 57},
                      {"non_square", 32, 64}};

    static const struct
    {
        const char *name;
        int width;
        int height;
        int depth;
    } tex3DSizes[] = {{nullptr, 32, 32, 32}, // Default.
                      {"npot", 33, 29, 27}};

    const int cubeMapSize = 64;

    static const struct
    {
        CoordType type;
        const char *name;
        const char *desc;
    } cubeCoordTypes[] = {{COORDTYPE_BASIC, "basic", "Mipmapping with translated and scaled coordinates"},
                          {COORDTYPE_PROJECTED, "projected", "Mipmapping with perspective projection"},
                          {COORDTYPE_BASIC_BIAS, "bias", "User-supplied bias value"}};

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
                // Add non_square variants to basic cases only.
                int sizeEnd = coordTypes[coordType].type == COORDTYPE_BASIC ? DE_LENGTH_OF_ARRAY(tex2DSizes) : 1;

                for (int size = 0; size < sizeEnd; size++)
                {
                    std::ostringstream name;
                    name << minFilterModes[minFilter].name << "_" << wrapModes[wrapMode].name;

                    if (tex2DSizes[size].name)
                        name << "_" << tex2DSizes[size].name;

                    coordTypeGroup->addChild(new Texture2DMipmapCase(
                        m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.str().c_str(), "",
                        coordTypes[coordType].type, minFilterModes[minFilter].mode, wrapModes[wrapMode].mode,
                        wrapModes[wrapMode].mode, GL_RGBA, GL_UNSIGNED_BYTE, tex2DSizes[size].width,
                        tex2DSizes[size].height));
                }
            }
        }
    }

    // 2D bias variants.
    {
        tcu::TestCaseGroup *biasGroup = new tcu::TestCaseGroup(m_testCtx, "bias", "User-supplied bias value");
        group2D->addChild(biasGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
            biasGroup->addChild(new Texture2DMipmapCase(
                m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), minFilterModes[minFilter].name, "",
                COORDTYPE_BASIC_BIAS, minFilterModes[minFilter].mode, GL_REPEAT, GL_REPEAT, GL_RGBA, GL_UNSIGNED_BYTE,
                tex2DSizes[0].width, tex2DSizes[0].height));
    }

    // 2D mipmap generation variants.
    {
        tcu::TestCaseGroup *genMipmapGroup = new tcu::TestCaseGroup(m_testCtx, "generate", "Mipmap generation tests");
        group2D->addChild(genMipmapGroup);

        for (int format = 0; format < DE_LENGTH_OF_ARRAY(formats); format++)
        {
            for (int size = 0; size < DE_LENGTH_OF_ARRAY(tex2DSizes); size++)
            {
                for (int hint = 0; hint < DE_LENGTH_OF_ARRAY(genHints); hint++)
                {
                    std::ostringstream name;
                    name << formats[format].name;

                    if (tex2DSizes[size].name)
                        name << "_" << tex2DSizes[size].name;

                    name << "_" << genHints[hint].name;

                    genMipmapGroup->addChild(new Texture2DGenMipmapCase(
                        m_testCtx, m_context.getRenderContext(), name.str().c_str(), "", formats[format].format,
                        formats[format].dataType, genHints[hint].hint, tex2DSizes[size].width,
                        tex2DSizes[size].height));
                }
            }
        }
    }

    // 2D LOD controls.
    {
        // MIN_LOD
        tcu::TestCaseGroup *minLodGroup = new tcu::TestCaseGroup(m_testCtx, "min_lod", "Lod control: min lod");
        group2D->addChild(minLodGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
            minLodGroup->addChild(
                new Texture2DMinLodCase(m_context, minFilterModes[minFilter].name, "", minFilterModes[minFilter].mode));

        // MAX_LOD
        tcu::TestCaseGroup *maxLodGroup = new tcu::TestCaseGroup(m_testCtx, "max_lod", "Lod control: max lod");
        group2D->addChild(maxLodGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
            maxLodGroup->addChild(
                new Texture2DMaxLodCase(m_context, minFilterModes[minFilter].name, "", minFilterModes[minFilter].mode));

        // BASE_LEVEL
        tcu::TestCaseGroup *baseLevelGroup = new tcu::TestCaseGroup(m_testCtx, "base_level", "Base level");
        group2D->addChild(baseLevelGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
            baseLevelGroup->addChild(new Texture2DBaseLevelCase(m_context, minFilterModes[minFilter].name, "",
                                                                minFilterModes[minFilter].mode));

        // MAX_LEVEL
        tcu::TestCaseGroup *maxLevelGroup = new tcu::TestCaseGroup(m_testCtx, "max_level", "Max level");
        group2D->addChild(maxLevelGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
            maxLevelGroup->addChild(new Texture2DMaxLevelCase(m_context, minFilterModes[minFilter].name, "",
                                                              minFilterModes[minFilter].mode));
    }

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

    // Cubemap mipmap generation variants.
    {
        tcu::TestCaseGroup *genMipmapGroup = new tcu::TestCaseGroup(m_testCtx, "generate", "Mipmap generation tests");
        groupCube->addChild(genMipmapGroup);

        for (int format = 0; format < DE_LENGTH_OF_ARRAY(formats); format++)
        {
            for (int hint = 0; hint < DE_LENGTH_OF_ARRAY(genHints); hint++)
            {
                std::ostringstream name;
                name << formats[format].name << "_" << genHints[hint].name;

                genMipmapGroup->addChild(new TextureCubeGenMipmapCase(
                    m_testCtx, m_context.getRenderContext(), name.str().c_str(), "", formats[format].format,
                    formats[format].dataType, genHints[hint].hint, cubeMapSize));
            }
        }
    }

    // Cubemap LOD controls.
    {
        // MIN_LOD
        tcu::TestCaseGroup *minLodGroup = new tcu::TestCaseGroup(m_testCtx, "min_lod", "Lod control: min lod");
        groupCube->addChild(minLodGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
            minLodGroup->addChild(new TextureCubeMinLodCase(m_context, minFilterModes[minFilter].name, "",
                                                            minFilterModes[minFilter].mode));

        // MAX_LOD
        tcu::TestCaseGroup *maxLodGroup = new tcu::TestCaseGroup(m_testCtx, "max_lod", "Lod control: max lod");
        groupCube->addChild(maxLodGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
            maxLodGroup->addChild(new TextureCubeMaxLodCase(m_context, minFilterModes[minFilter].name, "",
                                                            minFilterModes[minFilter].mode));

        // BASE_LEVEL
        tcu::TestCaseGroup *baseLevelGroup = new tcu::TestCaseGroup(m_testCtx, "base_level", "Base level");
        groupCube->addChild(baseLevelGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
            baseLevelGroup->addChild(new TextureCubeBaseLevelCase(m_context, minFilterModes[minFilter].name, "",
                                                                  minFilterModes[minFilter].mode));

        // MAX_LEVEL
        tcu::TestCaseGroup *maxLevelGroup = new tcu::TestCaseGroup(m_testCtx, "max_level", "Max level");
        groupCube->addChild(maxLevelGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
            maxLevelGroup->addChild(new TextureCubeMaxLevelCase(m_context, minFilterModes[minFilter].name, "",
                                                                minFilterModes[minFilter].mode));
    }

    // 3D cases.
    for (int coordType = 0; coordType < DE_LENGTH_OF_ARRAY(coordTypes); coordType++)
    {
        tcu::TestCaseGroup *coordTypeGroup =
            new tcu::TestCaseGroup(m_testCtx, coordTypes[coordType].name, coordTypes[coordType].desc);
        group3D->addChild(coordTypeGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
        {
            for (int wrapMode = 0; wrapMode < DE_LENGTH_OF_ARRAY(wrapModes); wrapMode++)
            {
                // Add other size variants to basic cases only.
                int sizeEnd = coordTypes[coordType].type == COORDTYPE_BASIC ? DE_LENGTH_OF_ARRAY(tex3DSizes) : 1;

                for (int size = 0; size < sizeEnd; size++)
                {
                    std::ostringstream name;
                    name << minFilterModes[minFilter].name << "_" << wrapModes[wrapMode].name;

                    if (tex3DSizes[size].name)
                        name << "_" << tex3DSizes[size].name;

                    coordTypeGroup->addChild(new Texture3DMipmapCase(
                        m_context, name.str().c_str(), "", coordTypes[coordType].type, minFilterModes[minFilter].mode,
                        wrapModes[wrapMode].mode, wrapModes[wrapMode].mode, wrapModes[wrapMode].mode, GL_RGBA8,
                        tex3DSizes[size].width, tex3DSizes[size].height, tex3DSizes[size].depth));
                }
            }
        }
    }

    // 3D bias variants.
    {
        tcu::TestCaseGroup *biasGroup = new tcu::TestCaseGroup(m_testCtx, "bias", "User-supplied bias value");
        group3D->addChild(biasGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
            biasGroup->addChild(new Texture3DMipmapCase(m_context, minFilterModes[minFilter].name, "",
                                                        COORDTYPE_BASIC_BIAS, minFilterModes[minFilter].mode, GL_REPEAT,
                                                        GL_REPEAT, GL_REPEAT, GL_RGBA8, tex3DSizes[0].width,
                                                        tex3DSizes[0].height, tex3DSizes[0].depth));
    }

    // 3D LOD controls.
    {
        // MIN_LOD
        tcu::TestCaseGroup *minLodGroup = new tcu::TestCaseGroup(m_testCtx, "min_lod", "Lod control: min lod");
        group3D->addChild(minLodGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
            minLodGroup->addChild(
                new Texture3DMinLodCase(m_context, minFilterModes[minFilter].name, "", minFilterModes[minFilter].mode));

        // MAX_LOD
        tcu::TestCaseGroup *maxLodGroup = new tcu::TestCaseGroup(m_testCtx, "max_lod", "Lod control: max lod");
        group3D->addChild(maxLodGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
            maxLodGroup->addChild(
                new Texture3DMaxLodCase(m_context, minFilterModes[minFilter].name, "", minFilterModes[minFilter].mode));

        // BASE_LEVEL
        tcu::TestCaseGroup *baseLevelGroup = new tcu::TestCaseGroup(m_testCtx, "base_level", "Base level");
        group3D->addChild(baseLevelGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
            baseLevelGroup->addChild(new Texture3DBaseLevelCase(m_context, minFilterModes[minFilter].name, "",
                                                                minFilterModes[minFilter].mode));

        // MAX_LEVEL
        tcu::TestCaseGroup *maxLevelGroup = new tcu::TestCaseGroup(m_testCtx, "max_level", "Max level");
        group3D->addChild(maxLevelGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
            maxLevelGroup->addChild(new Texture3DMaxLevelCase(m_context, minFilterModes[minFilter].name, "",
                                                              minFilterModes[minFilter].mode));
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
