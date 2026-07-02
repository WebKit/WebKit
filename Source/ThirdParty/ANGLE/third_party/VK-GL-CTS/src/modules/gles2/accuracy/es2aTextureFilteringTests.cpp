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
 * \brief Texture filtering accuracy tests.
 *//*--------------------------------------------------------------------*/

#include "es2aTextureFilteringTests.hpp"
#include "glsTextureTestUtil.hpp"
#include "gluTexture.hpp"
#include "gluStrUtil.hpp"
#include "gluTextureUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "tcuSurfaceAccess.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "deStringUtil.hpp"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

using std::string;

namespace deqp
{
namespace gles2
{
namespace Accuracy
{

using std::string;
using std::vector;
using tcu::Sampler;
using tcu::TestLog;
using namespace glu;
using namespace gls::TextureTestUtil;
using namespace glu::TextureTestUtil;

class Texture2DFilteringCase : public tcu::TestCase
{
public:
    Texture2DFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                           const char *name, const char *desc, uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                           uint32_t wrapT, uint32_t format, uint32_t dataType, int width, int height);
    Texture2DFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                           const char *name, const char *desc, uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                           uint32_t wrapT, const std::vector<std::string> &filenames);
    ~Texture2DFilteringCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture2DFilteringCase(const Texture2DFilteringCase &other);
    Texture2DFilteringCase &operator=(const Texture2DFilteringCase &other);

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    uint32_t m_minFilter;
    uint32_t m_magFilter;
    uint32_t m_wrapS;
    uint32_t m_wrapT;

    uint32_t m_format;
    uint32_t m_dataType;
    int m_width;
    int m_height;

    std::vector<std::string> m_filenames;

    std::vector<glu::Texture2D *> m_textures;
    TextureRenderer m_renderer;
};

Texture2DFilteringCase::Texture2DFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                               const glu::ContextInfo &ctxInfo, const char *name, const char *desc,
                                               uint32_t minFilter, uint32_t magFilter, uint32_t wrapS, uint32_t wrapT,
                                               uint32_t format, uint32_t dataType, int width, int height)
    : TestCase(testCtx, tcu::NODETYPE_ACCURACY, name, desc)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(ctxInfo)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_format(format)
    , m_dataType(dataType)
    , m_width(width)
    , m_height(height)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_100_ES, glu::PRECISION_MEDIUMP)
{
}

Texture2DFilteringCase::Texture2DFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                               const glu::ContextInfo &ctxInfo, const char *name, const char *desc,
                                               uint32_t minFilter, uint32_t magFilter, uint32_t wrapS, uint32_t wrapT,
                                               const std::vector<std::string> &filenames)
    : TestCase(testCtx, tcu::NODETYPE_ACCURACY, name, desc)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(ctxInfo)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_format(GL_NONE)
    , m_dataType(GL_NONE)
    , m_width(0)
    , m_height(0)
    , m_filenames(filenames)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_100_ES, glu::PRECISION_MEDIUMP)
{
}

Texture2DFilteringCase::~Texture2DFilteringCase(void)
{
    deinit();
}

void Texture2DFilteringCase::init(void)
{
    try
    {
        if (!m_filenames.empty())
        {
            m_textures.reserve(1);
            m_textures.push_back(glu::Texture2D::create(m_renderCtx, m_renderCtxInfo, m_testCtx.getArchive(),
                                                        (int)m_filenames.size(), m_filenames));
        }
        else
        {
            // Create 2 textures.
            m_textures.reserve(2);
            for (int ndx = 0; ndx < 2; ndx++)
                m_textures.push_back(new glu::Texture2D(m_renderCtx, m_format, m_dataType, m_width, m_height));

            const bool mipmaps             = deIsPowerOfTwo32(m_width) && deIsPowerOfTwo32(m_height);
            const int numLevels            = mipmaps ? deLog2Floor32(de::max(m_width, m_height)) + 1 : 1;
            tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(m_textures[0]->getRefTexture().getFormat());
            tcu::Vec4 cBias                = fmtInfo.valueMin;
            tcu::Vec4 cScale               = fmtInfo.valueMax - fmtInfo.valueMin;

            // Fill first gradient texture.
            for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
            {
                tcu::Vec4 gMin = tcu::Vec4(-0.5f, -0.5f, -0.5f, 2.0f) * cScale + cBias;
                tcu::Vec4 gMax = tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f) * cScale + cBias;

                m_textures[0]->getRefTexture().allocLevel(levelNdx);
                tcu::fillWithComponentGradients(m_textures[0]->getRefTexture().getLevel(levelNdx), gMin, gMax);
            }

            // Fill second with grid texture.
            for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
            {
                uint32_t step   = 0x00ffffff / numLevels;
                uint32_t rgb    = step * levelNdx;
                uint32_t colorA = 0xff000000 | rgb;
                uint32_t colorB = 0xff000000 | ~rgb;

                m_textures[1]->getRefTexture().allocLevel(levelNdx);
                tcu::fillWithGrid(m_textures[1]->getRefTexture().getLevel(levelNdx), 4,
                                  tcu::RGBA(colorA).toVec() * cScale + cBias,
                                  tcu::RGBA(colorB).toVec() * cScale + cBias);
            }

            // Upload.
            for (std::vector<glu::Texture2D *>::iterator i = m_textures.begin(); i != m_textures.end(); i++)
                (*i)->upload();
        }
    }
    catch (...)
    {
        // Clean up to save memory.
        Texture2DFilteringCase::deinit();
        throw;
    }
}

void Texture2DFilteringCase::deinit(void)
{
    for (std::vector<glu::Texture2D *>::iterator i = m_textures.begin(); i != m_textures.end(); i++)
        delete *i;
    m_textures.clear();

    m_renderer.clear();
}

Texture2DFilteringCase::IterateResult Texture2DFilteringCase::iterate(void)
{
    const glw::Functions &gl    = m_renderCtx.getFunctions();
    TestLog &log                = m_testCtx.getLog();
    const int defViewportWidth  = 256;
    const int defViewportHeight = 256;
    RandomViewport viewport(m_renderCtx.getRenderTarget(), defViewportWidth, defViewportHeight,
                            deStringHash(getName()));
    tcu::Surface renderedFrame(viewport.width, viewport.height);
    tcu::Surface referenceFrame(viewport.width, viewport.height);
    const tcu::TextureFormat &texFmt = m_textures[0]->getRefTexture().getFormat();
    tcu::TextureFormatInfo fmtInfo   = tcu::getTextureFormatInfo(texFmt);
    ReferenceParams refParams(TEXTURETYPE_2D);
    vector<float> texCoord;

    // Accuracy measurements are off unless viewport size is 256x256
    if (viewport.width < defViewportWidth || viewport.height < defViewportHeight)
        throw tcu::NotSupportedError("Too small viewport", "", __FILE__, __LINE__);

    // Viewport is divided into 4 sections.
    int leftWidth    = viewport.width / 2;
    int rightWidth   = viewport.width - leftWidth;
    int bottomHeight = viewport.height / 2;
    int topHeight    = viewport.height - bottomHeight;

    int curTexNdx = 0;

    // Use unit 0.
    gl.activeTexture(GL_TEXTURE0);

    // Bind gradient texture and setup sampler parameters.
    gl.bindTexture(GL_TEXTURE_2D, m_textures[curTexNdx]->getGLTexture());
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapS);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapT);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_magFilter);

    // Setup params for reference.
    refParams.sampler     = mapGLSampler(m_wrapS, m_wrapT, m_minFilter, m_magFilter);
    refParams.samplerType = getSamplerType(texFmt);
    refParams.lodMode     = LODMODE_EXACT;
    refParams.colorBias   = fmtInfo.lookupBias;
    refParams.colorScale  = fmtInfo.lookupScale;

    // Bottom left: Minification
    {
        gl.viewport(viewport.x, viewport.y, leftWidth, bottomHeight);

        computeQuadTexCoord2D(texCoord, tcu::Vec2(-4.0f, -4.5f), tcu::Vec2(4.0f, 2.5f));

        m_renderer.renderQuad(0, &texCoord[0], refParams);
        sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat(), 0, 0,
                                         leftWidth, bottomHeight),
                      m_textures[curTexNdx]->getRefTexture(), &texCoord[0], refParams);
    }

    // Bottom right: Magnification
    {
        gl.viewport(viewport.x + leftWidth, viewport.y, rightWidth, bottomHeight);

        computeQuadTexCoord2D(texCoord, tcu::Vec2(-0.5f, 0.75f), tcu::Vec2(0.25f, 1.25f));

        m_renderer.renderQuad(0, &texCoord[0], refParams);
        sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat(), leftWidth, 0,
                                         rightWidth, bottomHeight),
                      m_textures[curTexNdx]->getRefTexture(), &texCoord[0], refParams);
    }

    if (m_textures.size() >= 2)
    {
        curTexNdx += 1;

        // Setup second texture.
        gl.bindTexture(GL_TEXTURE_2D, m_textures[curTexNdx]->getGLTexture());
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapS);
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapT);
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_minFilter);
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_magFilter);
    }

    // Top left: Minification
    // \note Minification is chosen so that 0.0 < lod <= 0.5. This way special minification threshold rule will be triggered.
    {
        gl.viewport(viewport.x, viewport.y + bottomHeight, leftWidth, topHeight);

        float sMin   = -0.5f;
        float tMin   = -0.2f;
        float sRange = ((float)leftWidth * 1.2f) / (float)m_textures[curTexNdx]->getRefTexture().getWidth();
        float tRange = ((float)topHeight * 1.1f) / (float)m_textures[curTexNdx]->getRefTexture().getHeight();

        computeQuadTexCoord2D(texCoord, tcu::Vec2(sMin, tMin), tcu::Vec2(sMin + sRange, tMin + tRange));

        m_renderer.renderQuad(0, &texCoord[0], refParams);
        sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat(), 0,
                                         bottomHeight, leftWidth, topHeight),
                      m_textures[curTexNdx]->getRefTexture(), &texCoord[0], refParams);
    }

    // Top right: Magnification
    {
        gl.viewport(viewport.x + leftWidth, viewport.y + bottomHeight, rightWidth, topHeight);

        computeQuadTexCoord2D(texCoord, tcu::Vec2(-0.5f, 0.75f), tcu::Vec2(0.25f, 1.25f));

        m_renderer.renderQuad(0, &texCoord[0], refParams);
        sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat(), leftWidth,
                                         bottomHeight, rightWidth, topHeight),
                      m_textures[curTexNdx]->getRefTexture(), &texCoord[0], refParams);
    }

    // Read result.
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());

    // Compare and log.
    {
        DE_ASSERT(getNodeType() == tcu::NODETYPE_ACCURACY);

        const int bestScoreDiff  = 16;
        const int worstScoreDiff = 3200;

        int score = measureAccuracy(log, referenceFrame, renderedFrame, bestScoreDiff, worstScoreDiff);
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::toString(score).c_str());
    }

    return STOP;
}

class TextureCubeFilteringCase : public tcu::TestCase
{
public:
    TextureCubeFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                             const char *name, const char *desc, uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                             uint32_t wrapT, uint32_t format, uint32_t dataType, int width, int height);
    TextureCubeFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                             const char *name, const char *desc, uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                             uint32_t wrapT, const std::vector<std::string> &filenames);
    ~TextureCubeFilteringCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    TextureCubeFilteringCase(const TextureCubeFilteringCase &other);
    TextureCubeFilteringCase &operator=(const TextureCubeFilteringCase &other);

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    uint32_t m_minFilter;
    uint32_t m_magFilter;
    uint32_t m_wrapS;
    uint32_t m_wrapT;

    uint32_t m_format;
    uint32_t m_dataType;
    int m_width;
    int m_height;

    std::vector<std::string> m_filenames;

    std::vector<glu::TextureCube *> m_textures;
    TextureRenderer m_renderer;
};

TextureCubeFilteringCase::TextureCubeFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                   const glu::ContextInfo &ctxInfo, const char *name, const char *desc,
                                                   uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                                                   uint32_t wrapT, uint32_t format, uint32_t dataType, int width,
                                                   int height)
    : TestCase(testCtx, tcu::NODETYPE_ACCURACY, name, desc)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(ctxInfo)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_format(format)
    , m_dataType(dataType)
    , m_width(width)
    , m_height(height)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_100_ES, glu::PRECISION_MEDIUMP)
{
}

TextureCubeFilteringCase::TextureCubeFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                   const glu::ContextInfo &ctxInfo, const char *name, const char *desc,
                                                   uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                                                   uint32_t wrapT, const std::vector<std::string> &filenames)
    : TestCase(testCtx, tcu::NODETYPE_ACCURACY, name, desc)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(ctxInfo)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_format(GL_NONE)
    , m_dataType(GL_NONE)
    , m_width(0)
    , m_height(0)
    , m_filenames(filenames)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_100_ES, glu::PRECISION_MEDIUMP)
{
}

TextureCubeFilteringCase::~TextureCubeFilteringCase(void)
{
    deinit();
}

void TextureCubeFilteringCase::init(void)
{
    try
    {
        if (!m_filenames.empty())
        {
            m_textures.reserve(1);
            m_textures.push_back(glu::TextureCube::create(m_renderCtx, m_renderCtxInfo, m_testCtx.getArchive(),
                                                          (int)m_filenames.size() / 6, m_filenames));
        }
        else
        {
            m_textures.reserve(2);
            DE_ASSERT(m_width == m_height);
            for (int ndx = 0; ndx < 2; ndx++)
                m_textures.push_back(new glu::TextureCube(m_renderCtx, m_format, m_dataType, m_width));

            const bool mipmaps             = deIsPowerOfTwo32(m_width) && deIsPowerOfTwo32(m_height);
            const int numLevels            = mipmaps ? deLog2Floor32(de::max(m_width, m_height)) + 1 : 1;
            tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(m_textures[0]->getRefTexture().getFormat());
            tcu::Vec4 cBias                = fmtInfo.valueMin;
            tcu::Vec4 cScale               = fmtInfo.valueMax - fmtInfo.valueMin;

            // Fill first with gradient texture.
            static const tcu::Vec4 gradients[tcu::CUBEFACE_LAST][2] = {
                {tcu::Vec4(-1.0f, -1.0f, -1.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)}, // negative x
                {tcu::Vec4(0.0f, -1.0f, -1.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)},  // positive x
                {tcu::Vec4(-1.0f, 0.0f, -1.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)},  // negative y
                {tcu::Vec4(-1.0f, -1.0f, 0.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)},  // positive y
                {tcu::Vec4(-1.0f, -1.0f, -1.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f)}, // negative z
                {tcu::Vec4(0.0f, 0.0f, 0.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)}     // positive z
            };
            for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
            {
                for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
                {
                    m_textures[0]->getRefTexture().allocLevel((tcu::CubeFace)face, levelNdx);
                    tcu::fillWithComponentGradients(
                        m_textures[0]->getRefTexture().getLevelFace(levelNdx, (tcu::CubeFace)face),
                        gradients[face][0] * cScale + cBias, gradients[face][1] * cScale + cBias);
                }
            }

            // Fill second with grid texture.
            for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
            {
                for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
                {
                    uint32_t step   = 0x00ffffff / (numLevels * tcu::CUBEFACE_LAST);
                    uint32_t rgb    = step * levelNdx * face;
                    uint32_t colorA = 0xff000000 | rgb;
                    uint32_t colorB = 0xff000000 | ~rgb;

                    m_textures[1]->getRefTexture().allocLevel((tcu::CubeFace)face, levelNdx);
                    tcu::fillWithGrid(m_textures[1]->getRefTexture().getLevelFace(levelNdx, (tcu::CubeFace)face), 4,
                                      tcu::RGBA(colorA).toVec() * cScale + cBias,
                                      tcu::RGBA(colorB).toVec() * cScale + cBias);
                }
            }

            // Upload.
            for (std::vector<glu::TextureCube *>::iterator i = m_textures.begin(); i != m_textures.end(); i++)
                (*i)->upload();
        }
    }
    catch (const std::exception &)
    {
        // Clean up to save memory.
        TextureCubeFilteringCase::deinit();
        throw;
    }
}

void TextureCubeFilteringCase::deinit(void)
{
    for (std::vector<glu::TextureCube *>::iterator i = m_textures.begin(); i != m_textures.end(); i++)
        delete *i;
    m_textures.clear();

    m_renderer.clear();
}

static void renderFaces(const glw::Functions &gl, const tcu::SurfaceAccess &dstRef, const tcu::TextureCube &refTexture,
                        const ReferenceParams &params, TextureRenderer &renderer, int x, int y, int width, int height,
                        const tcu::Vec2 &bottomLeft, const tcu::Vec2 &topRight, const tcu::Vec2 &texCoordTopRightFactor)
{
    DE_ASSERT(width == dstRef.getWidth() && height == dstRef.getHeight());

    vector<float> texCoord;

    DE_STATIC_ASSERT(tcu::CUBEFACE_LAST == 6);
    for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
    {
        bool isRightmost = (face == 2) || (face == 5);
        bool isTop       = face >= 3;
        int curX         = (face % 3) * (width / 3);
        int curY         = (face / 3) * (height / 2);
        int curW         = isRightmost ? (width - curX) : (width / 3);
        int curH         = isTop ? (height - curY) : (height / 2);

        computeQuadTexCoordCube(texCoord, (tcu::CubeFace)face, bottomLeft, topRight);

        {
            // Move the top and right edges of the texture coord quad. This is useful when we want a cube edge visible.
            int texCoordSRow = face == tcu::CUBEFACE_NEGATIVE_X || face == tcu::CUBEFACE_POSITIVE_X ? 2 : 0;
            int texCoordTRow = face == tcu::CUBEFACE_NEGATIVE_Y || face == tcu::CUBEFACE_POSITIVE_Y ? 2 : 1;
            texCoord[6 + texCoordSRow] *= texCoordTopRightFactor.x();
            texCoord[9 + texCoordSRow] *= texCoordTopRightFactor.x();
            texCoord[3 + texCoordTRow] *= texCoordTopRightFactor.y();
            texCoord[9 + texCoordTRow] *= texCoordTopRightFactor.y();
        }

        gl.viewport(x + curX, y + curY, curW, curH);

        renderer.renderQuad(0, &texCoord[0], params);

        sampleTexture(tcu::SurfaceAccess(dstRef, curX, curY, curW, curH), refTexture, &texCoord[0], params);
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Post render");
}

TextureCubeFilteringCase::IterateResult TextureCubeFilteringCase::iterate(void)
{
    const glw::Functions &gl    = m_renderCtx.getFunctions();
    TestLog &log                = m_testCtx.getLog();
    const int cellSize          = 28;
    const int defViewportWidth  = cellSize * 6;
    const int defViewportHeight = cellSize * 4;
    RandomViewport viewport(m_renderCtx.getRenderTarget(), cellSize * 6, cellSize * 4, deStringHash(getName()));
    tcu::Surface renderedFrame(viewport.width, viewport.height);
    tcu::Surface referenceFrame(viewport.width, viewport.height);
    ReferenceParams sampleParams(TEXTURETYPE_CUBE);
    const tcu::TextureFormat &texFmt = m_textures[0]->getRefTexture().getFormat();
    tcu::TextureFormatInfo fmtInfo   = tcu::getTextureFormatInfo(texFmt);

    // Accuracy measurements are off unless viewport size is exactly as expected.
    if (viewport.width < defViewportWidth || viewport.height < defViewportHeight)
        throw tcu::NotSupportedError("Too small viewport", "", __FILE__, __LINE__);

    // Viewport is divided into 4 sections.
    int leftWidth    = viewport.width / 2;
    int rightWidth   = viewport.width - leftWidth;
    int bottomHeight = viewport.height / 2;
    int topHeight    = viewport.height - bottomHeight;

    int curTexNdx = 0;

    // Sampling parameters.
    sampleParams.sampler                 = mapGLSampler(m_wrapS, m_wrapT, m_minFilter, m_magFilter);
    sampleParams.sampler.seamlessCubeMap = false;
    sampleParams.samplerType             = getSamplerType(texFmt);
    sampleParams.colorBias               = fmtInfo.lookupBias;
    sampleParams.colorScale              = fmtInfo.lookupScale;
    sampleParams.lodMode                 = LODMODE_EXACT;

    // Use unit 0.
    gl.activeTexture(GL_TEXTURE0);

    // Setup gradient texture.
    gl.bindTexture(GL_TEXTURE_CUBE_MAP, m_textures[curTexNdx]->getGLTexture());
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, m_wrapS);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, m_wrapT);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, m_magFilter);

    // Bottom left: Minification
    renderFaces(gl,
                tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat(), 0, 0, leftWidth,
                                   bottomHeight),
                m_textures[curTexNdx]->getRefTexture(), sampleParams, m_renderer, viewport.x, viewport.y, leftWidth,
                bottomHeight, tcu::Vec2(-0.81f, -0.81f), tcu::Vec2(0.8f, 0.8f), tcu::Vec2(1.0f, 1.0f));

    // Bottom right: Magnification
    renderFaces(gl,
                tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat(), leftWidth, 0,
                                   rightWidth, bottomHeight),
                m_textures[curTexNdx]->getRefTexture(), sampleParams, m_renderer, viewport.x + leftWidth, viewport.y,
                rightWidth, bottomHeight, tcu::Vec2(0.5f, 0.65f), tcu::Vec2(0.8f, 0.8f), tcu::Vec2(1.0f, 1.0f));

    if (m_textures.size() >= 2)
    {
        curTexNdx += 1;

        // Setup second texture.
        gl.bindTexture(GL_TEXTURE_CUBE_MAP, m_textures[curTexNdx]->getGLTexture());
        gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, m_wrapS);
        gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, m_wrapT);
        gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, m_minFilter);
        gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, m_magFilter);
    }

    // Top left: Minification
    renderFaces(gl,
                tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat(), 0, bottomHeight,
                                   leftWidth, topHeight),
                m_textures[curTexNdx]->getRefTexture(), sampleParams, m_renderer, viewport.x, viewport.y + bottomHeight,
                leftWidth, topHeight, tcu::Vec2(-0.81f, -0.81f), tcu::Vec2(0.8f, 0.8f), tcu::Vec2(1.0f, 1.0f));

    // Top right: Magnification
    renderFaces(gl,
                tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat(), leftWidth,
                                   bottomHeight, rightWidth, topHeight),
                m_textures[curTexNdx]->getRefTexture(), sampleParams, m_renderer, viewport.x + leftWidth,
                viewport.y + bottomHeight, rightWidth, topHeight, tcu::Vec2(0.5f, -0.65f), tcu::Vec2(0.8f, -0.8f),
                tcu::Vec2(1.0f, 1.0f));

    // Read result.
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());

    // Compare and log.
    {
        DE_ASSERT(getNodeType() == tcu::NODETYPE_ACCURACY);

        const int bestScoreDiff  = 16;
        const int worstScoreDiff = 10000;

        int score = measureAccuracy(log, referenceFrame, renderedFrame, bestScoreDiff, worstScoreDiff);
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::toString(score).c_str());
    }

    return STOP;
}

TextureFilteringTests::TextureFilteringTests(Context &context)
    : TestCaseGroup(context, "filter", "Texture Filtering Accuracy Tests")
{
}

TextureFilteringTests::~TextureFilteringTests(void)
{
}

void TextureFilteringTests::init(void)
{
    tcu::TestCaseGroup *group2D   = new tcu::TestCaseGroup(m_testCtx, "2d", "2D Texture Filtering");
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
    } minFilterModes[] = {{"nearest", GL_NEAREST},
                          {"linear", GL_LINEAR},
                          {"nearest_mipmap_nearest", GL_NEAREST_MIPMAP_NEAREST},
                          {"linear_mipmap_nearest", GL_LINEAR_MIPMAP_NEAREST},
                          {"nearest_mipmap_linear", GL_NEAREST_MIPMAP_LINEAR},
                          {"linear_mipmap_linear", GL_LINEAR_MIPMAP_LINEAR}};

    static const struct
    {
        const char *name;
        uint32_t mode;
    } magFilterModes[] = {{"nearest", GL_NEAREST}, {"linear", GL_LINEAR}};

    static const struct
    {
        const char *name;
        int width;
        int height;
    } sizes2D[] = {{"pot", 32, 64}, {"npot", 31, 55}};

    static const struct
    {
        const char *name;
        int width;
        int height;
    } sizesCube[] = {{"pot", 64, 64}, {"npot", 63, 63}};

    static const struct
    {
        const char *name;
        uint32_t format;
        uint32_t dataType;
    } formats[] = {{"rgba8888", GL_RGBA, GL_UNSIGNED_BYTE}, {"rgba4444", GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4}};

#define FOR_EACH(ITERATOR, ARRAY, BODY)                                      \
    for (int ITERATOR = 0; ITERATOR < DE_LENGTH_OF_ARRAY(ARRAY); ITERATOR++) \
    BODY

    // 2D cases.
    FOR_EACH(minFilter, minFilterModes,
             FOR_EACH(magFilter, magFilterModes,
                      FOR_EACH(wrapMode, wrapModes,
                               FOR_EACH(format, formats, FOR_EACH(size, sizes2D, {
                                            bool isMipmap = minFilterModes[minFilter].mode != GL_NEAREST &&
                                                            minFilterModes[minFilter].mode != GL_LINEAR;
                                            bool isClamp      = wrapModes[wrapMode].mode == GL_CLAMP_TO_EDGE;
                                            bool isRepeat     = wrapModes[wrapMode].mode == GL_REPEAT;
                                            bool isMagNearest = magFilterModes[magFilter].mode == GL_NEAREST;
                                            bool isPotSize    = deIsPowerOfTwo32(sizes2D[size].width) &&
                                                             deIsPowerOfTwo32(sizes2D[size].height);

                                            if ((isMipmap || !isClamp) && !isPotSize)
                                                continue; // Not supported.

                                            if ((format != 0) && !(!isMipmap || (isRepeat && isMagNearest)))
                                                continue; // Skip.

                                            string name = string("") + minFilterModes[minFilter].name + "_" +
                                                          magFilterModes[magFilter].name + "_" +
                                                          wrapModes[wrapMode].name + "_" + formats[format].name;

                                            if (!isMipmap)
                                                name += string("_") + sizes2D[size].name;

                                            group2D->addChild(new Texture2DFilteringCase(
                                                m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(),
                                                name.c_str(), "", minFilterModes[minFilter].mode,
                                                magFilterModes[magFilter].mode, wrapModes[wrapMode].mode,
                                                wrapModes[wrapMode].mode, formats[format].format,
                                                formats[format].dataType, sizes2D[size].width, sizes2D[size].height));
                                        })))))

    // Cubemap cases.
    FOR_EACH(minFilter, minFilterModes,
             FOR_EACH(magFilter, magFilterModes,
                      FOR_EACH(wrapMode, wrapModes,
                               FOR_EACH(format, formats, FOR_EACH(size, sizesCube, {
                                            bool isMipmap = minFilterModes[minFilter].mode != GL_NEAREST &&
                                                            minFilterModes[minFilter].mode != GL_LINEAR;
                                            bool isClamp      = wrapModes[wrapMode].mode == GL_CLAMP_TO_EDGE;
                                            bool isRepeat     = wrapModes[wrapMode].mode == GL_REPEAT;
                                            bool isMagNearest = magFilterModes[magFilter].mode == GL_NEAREST;
                                            bool isPotSize    = deIsPowerOfTwo32(sizesCube[size].width) &&
                                                             deIsPowerOfTwo32(sizesCube[size].height);

                                            if ((isMipmap || !isClamp) && !isPotSize)
                                                continue; // Not supported.

                                            if (format != 0 && !(!isMipmap || (isRepeat && isMagNearest)))
                                                continue; // Skip.

                                            string name = string("") + minFilterModes[minFilter].name + "_" +
                                                          magFilterModes[magFilter].name + "_" +
                                                          wrapModes[wrapMode].name + "_" + formats[format].name;

                                            if (!isMipmap)
                                                name += string("_") + sizesCube[size].name;

                                            groupCube->addChild(new TextureCubeFilteringCase(
                                                m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(),
                                                name.c_str(), "", minFilterModes[minFilter].mode,
                                                magFilterModes[magFilter].mode, wrapModes[wrapMode].mode,
                                                wrapModes[wrapMode].mode, formats[format].format,
                                                formats[format].dataType, sizesCube[size].width,
                                                sizesCube[size].height));
                                        })))))
}

} // namespace Accuracy
} // namespace gles2
} // namespace deqp
