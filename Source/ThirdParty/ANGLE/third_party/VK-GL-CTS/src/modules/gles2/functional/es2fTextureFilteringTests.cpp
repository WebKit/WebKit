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
 * \brief Texture filtering tests.
 *//*--------------------------------------------------------------------*/

#include "es2fTextureFilteringTests.hpp"
#include "glsTextureTestUtil.hpp"
#include "gluTexture.hpp"
#include "gluStrUtil.hpp"
#include "gluTextureUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTexLookupVerifier.hpp"
#include "tcuVectorUtil.hpp"
#include "deStringUtil.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles2
{
namespace Functional
{

using std::string;
using std::vector;
using tcu::Sampler;
using tcu::TestLog;
using namespace glu;
using namespace gls::TextureTestUtil;
using namespace glu::TextureTestUtil;

enum
{
    VIEWPORT_WIDTH      = 64,
    VIEWPORT_HEIGHT     = 64,
    MIN_VIEWPORT_WIDTH  = 64,
    MIN_VIEWPORT_HEIGHT = 64
};

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

    const uint32_t m_minFilter;
    const uint32_t m_magFilter;
    const uint32_t m_wrapS;
    const uint32_t m_wrapT;

    const uint32_t m_format;
    const uint32_t m_dataType;
    const int m_width;
    const int m_height;

    const std::vector<std::string> m_filenames;

    struct FilterCase
    {
        const glu::Texture2D *texture;
        tcu::Vec2 minCoord;
        tcu::Vec2 maxCoord;

        FilterCase(void) : texture(nullptr)
        {
        }

        FilterCase(const glu::Texture2D *tex_, const tcu::Vec2 &minCoord_, const tcu::Vec2 &maxCoord_)
            : texture(tex_)
            , minCoord(minCoord_)
            , maxCoord(maxCoord_)
        {
        }
    };

    std::vector<glu::Texture2D *> m_textures;
    std::vector<FilterCase> m_cases;

    TextureRenderer m_renderer;

    int m_caseNdx;
};

Texture2DFilteringCase::Texture2DFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                               const glu::ContextInfo &ctxInfo, const char *name, const char *desc,
                                               uint32_t minFilter, uint32_t magFilter, uint32_t wrapS, uint32_t wrapT,
                                               uint32_t format, uint32_t dataType, int width, int height)
    : TestCase(testCtx, name, desc)
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
    , m_caseNdx(0)
{
}

Texture2DFilteringCase::Texture2DFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                               const glu::ContextInfo &ctxInfo, const char *name, const char *desc,
                                               uint32_t minFilter, uint32_t magFilter, uint32_t wrapS, uint32_t wrapT,
                                               const std::vector<std::string> &filenames)
    : TestCase(testCtx, name, desc)
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
    , m_caseNdx(0)
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

            bool mipmaps                   = deIsPowerOfTwo32(m_width) && deIsPowerOfTwo32(m_height);
            int numLevels                  = mipmaps ? deLog2Floor32(de::max(m_width, m_height)) + 1 : 1;
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

        // Compute cases.
        {
            const struct
            {
                int texNdx;
                float lodX;
                float lodY;
                float oX;
                float oY;
            } cases[] = {
                {0, 1.6f, 2.9f, -1.0f, -2.7f},
                {0, -2.0f, -1.35f, -0.2f, 0.7f},
                {1, 0.14f, 0.275f, -1.5f, -1.1f},
                {1, -0.92f, -2.64f, 0.4f, -0.1f},
            };

            const float viewportW = (float)de::min<int>(VIEWPORT_WIDTH, m_renderCtx.getRenderTarget().getWidth());
            const float viewportH = (float)de::min<int>(VIEWPORT_HEIGHT, m_renderCtx.getRenderTarget().getHeight());

            for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); caseNdx++)
            {
                const int texNdx = de::clamp(cases[caseNdx].texNdx, 0, (int)m_textures.size() - 1);
                const float lodX = cases[caseNdx].lodX;
                const float lodY = cases[caseNdx].lodY;
                const float oX   = cases[caseNdx].oX;
                const float oY   = cases[caseNdx].oY;
                const float sX = deFloatExp2(lodX) * viewportW / float(m_textures[texNdx]->getRefTexture().getWidth());
                const float sY = deFloatExp2(lodY) * viewportH / float(m_textures[texNdx]->getRefTexture().getHeight());

                m_cases.push_back(FilterCase(m_textures[texNdx], tcu::Vec2(oX, oY), tcu::Vec2(oX + sX, oY + sY)));
            }
        }

        m_caseNdx = 0;
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
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
    m_cases.clear();
}

Texture2DFilteringCase::IterateResult Texture2DFilteringCase::iterate(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    const RandomViewport viewport(m_renderCtx.getRenderTarget(), VIEWPORT_WIDTH, VIEWPORT_HEIGHT,
                                  deStringHash(getName()) ^ deInt32Hash(m_caseNdx));
    const tcu::TextureFormat texFmt      = m_textures[0]->getRefTexture().getFormat();
    const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);
    const FilterCase &curCase            = m_cases[m_caseNdx];
    const tcu::ScopedLogSection section(m_testCtx.getLog(), string("Test") + de::toString(m_caseNdx),
                                        string("Test ") + de::toString(m_caseNdx));
    ReferenceParams refParams(TEXTURETYPE_2D);
    tcu::Surface rendered(viewport.width, viewport.height);
    vector<float> texCoord;

    if (viewport.width < MIN_VIEWPORT_WIDTH || viewport.height < MIN_VIEWPORT_HEIGHT)
        throw tcu::NotSupportedError("Too small viewport", "", __FILE__, __LINE__);

    // Setup params for reference.
    refParams.sampler     = mapGLSampler(m_wrapS, m_wrapT, m_minFilter, m_magFilter);
    refParams.samplerType = getSamplerType(texFmt);
    refParams.lodMode     = LODMODE_EXACT;
    refParams.colorBias   = fmtInfo.lookupBias;
    refParams.colorScale  = fmtInfo.lookupScale;

    // Compute texture coordinates.
    m_testCtx.getLog() << TestLog::Message << "Texture coordinates: " << curCase.minCoord << " -> " << curCase.maxCoord
                       << TestLog::EndMessage;
    computeQuadTexCoord2D(texCoord, curCase.minCoord, curCase.maxCoord);

    gl.bindTexture(GL_TEXTURE_2D, curCase.texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_magFilter);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapS);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapT);

    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);
    m_renderer.renderQuad(0, &texCoord[0], refParams);
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, rendered.getAccess());

    {
        const bool isNearestOnly           = m_minFilter == GL_NEAREST && m_magFilter == GL_NEAREST;
        const tcu::PixelFormat pixelFormat = m_renderCtx.getRenderTarget().getPixelFormat();
        const tcu::IVec4 colorBits         = max(getBitsVec(pixelFormat) - (isNearestOnly ? 1 : 2),
                                                 tcu::IVec4(0)); // 1 inaccurate bit if nearest only, 2 otherwise
        tcu::LodPrecision lodPrecision;
        tcu::LookupPrecision lookupPrecision;

        lodPrecision.derivateBits      = 7;
        lodPrecision.lodBits           = 4;
        lookupPrecision.colorThreshold = tcu::computeFixedPointThreshold(colorBits) / refParams.colorScale;
        lookupPrecision.coordBits      = tcu::IVec3(9, 9, 0); // mediump interpolation
        lookupPrecision.uvwBits        = tcu::IVec3(5, 5, 0);
        lookupPrecision.colorMask      = getCompareMask(pixelFormat);

        const bool isOk = verifyTextureResult(m_testCtx, rendered.getAccess(), curCase.texture->getRefTexture(),
                                              &texCoord[0], refParams, lookupPrecision, lodPrecision, pixelFormat);

        if (!isOk)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image verification failed");
    }

    m_caseNdx += 1;
    return m_caseNdx < (int)m_cases.size() ? CONTINUE : STOP;
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

    const uint32_t m_minFilter;
    const uint32_t m_magFilter;
    const uint32_t m_wrapS;
    const uint32_t m_wrapT;

    const uint32_t m_format;
    const uint32_t m_dataType;
    const int m_width;
    const int m_height;

    const std::vector<std::string> m_filenames;

    struct FilterCase
    {
        const glu::TextureCube *texture;
        tcu::Vec2 bottomLeft;
        tcu::Vec2 topRight;

        FilterCase(void) : texture(nullptr)
        {
        }

        FilterCase(const glu::TextureCube *tex_, const tcu::Vec2 &bottomLeft_, const tcu::Vec2 &topRight_)
            : texture(tex_)
            , bottomLeft(bottomLeft_)
            , topRight(topRight_)
        {
        }
    };

    std::vector<glu::TextureCube *> m_textures;
    std::vector<FilterCase> m_cases;

    TextureRenderer m_renderer;

    int m_caseNdx;
};

TextureCubeFilteringCase::TextureCubeFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                   const glu::ContextInfo &ctxInfo, const char *name, const char *desc,
                                                   uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                                                   uint32_t wrapT, uint32_t format, uint32_t dataType, int width,
                                                   int height)
    : TestCase(testCtx, name, desc)
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
    , m_caseNdx(0)
{
}

TextureCubeFilteringCase::TextureCubeFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                   const glu::ContextInfo &ctxInfo, const char *name, const char *desc,
                                                   uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                                                   uint32_t wrapT, const std::vector<std::string> &filenames)
    : TestCase(testCtx, name, desc)
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
    , m_caseNdx(0)
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
            DE_ASSERT(m_width == m_height);
            m_textures.reserve(2);
            for (int ndx = 0; ndx < 2; ndx++)
                m_textures.push_back(new glu::TextureCube(m_renderCtx, m_format, m_dataType, m_width));

            const bool mipmaps             = deIsPowerOfTwo32(m_width) && deIsPowerOfTwo32(m_height);
            const int numLevels            = mipmaps ? deLog2Floor32(de::max(m_width, m_height)) + 1 : 1;
            tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(m_textures[0]->getRefTexture().getFormat());
            tcu::Vec4 cBias                = fmtInfo.valueMin;
            tcu::Vec4 cScale               = fmtInfo.valueMax - fmtInfo.valueMin;

            // Fill first with gradient texture.
            static const tcu::Vec4 gradients[tcu::CUBEFACE_LAST][2] = {
                {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)}, // negative x
                {tcu::Vec4(0.5f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)}, // positive x
                {tcu::Vec4(0.0f, 0.5f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)}, // negative y
                {tcu::Vec4(0.0f, 0.0f, 0.5f, 1.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)}, // positive y
                {tcu::Vec4(0.0f, 0.0f, 0.0f, 0.5f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f)}, // negative z
                {tcu::Vec4(0.5f, 0.5f, 0.5f, 1.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)}  // positive z
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

        // Compute cases
        {
            const glu::TextureCube *tex0 = m_textures[0];
            const glu::TextureCube *tex1 = m_textures.size() > 1 ? m_textures[1] : tex0;

            // \note Coordinates are chosen so that they only sample face interior. ES3 has changed edge sampling behavior
            //         and hw is not expected to implement both modes.
            m_cases.push_back(FilterCase(tex0, tcu::Vec2(-0.8f, -0.8f), tcu::Vec2(0.8f, 0.8f))); // minification
            m_cases.push_back(FilterCase(tex0, tcu::Vec2(0.5f, 0.65f), tcu::Vec2(0.8f, 0.8f)));  // magnification
            m_cases.push_back(FilterCase(tex1, tcu::Vec2(-0.8f, -0.8f), tcu::Vec2(0.8f, 0.8f))); // minification
            m_cases.push_back(FilterCase(tex1, tcu::Vec2(0.2f, 0.2f), tcu::Vec2(0.6f, 0.5f)));   // magnification
        }

        m_caseNdx = 0;
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }
    catch (...)
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
    m_cases.clear();
}

static const char *getFaceDesc(const tcu::CubeFace face)
{
    switch (face)
    {
    case tcu::CUBEFACE_NEGATIVE_X:
        return "-X";
    case tcu::CUBEFACE_POSITIVE_X:
        return "+X";
    case tcu::CUBEFACE_NEGATIVE_Y:
        return "-Y";
    case tcu::CUBEFACE_POSITIVE_Y:
        return "+Y";
    case tcu::CUBEFACE_NEGATIVE_Z:
        return "-Z";
    case tcu::CUBEFACE_POSITIVE_Z:
        return "+Z";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

TextureCubeFilteringCase::IterateResult TextureCubeFilteringCase::iterate(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    const int viewportSize   = 28;
    const RandomViewport viewport(m_renderCtx.getRenderTarget(), viewportSize, viewportSize,
                                  deStringHash(getName()) ^ deInt32Hash(m_caseNdx));
    const tcu::ScopedLogSection iterSection(m_testCtx.getLog(), string("Test") + de::toString(m_caseNdx),
                                            string("Test ") + de::toString(m_caseNdx));
    const FilterCase &curCase            = m_cases[m_caseNdx];
    const tcu::TextureFormat &texFmt     = curCase.texture->getRefTexture().getFormat();
    const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);
    ReferenceParams sampleParams(TEXTURETYPE_CUBE);

    if (viewport.width < viewportSize || viewport.height < viewportSize)
        throw tcu::NotSupportedError("Too small render target", nullptr, __FILE__, __LINE__);

    // Setup texture
    gl.bindTexture(GL_TEXTURE_CUBE_MAP, curCase.texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, m_magFilter);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, m_wrapS);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, m_wrapT);

    // Other state
    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

    // Params for reference computation.
    sampleParams.sampler                 = glu::mapGLSampler(m_wrapS, m_wrapT, m_minFilter, m_magFilter);
    sampleParams.sampler.seamlessCubeMap = true;
    sampleParams.samplerType             = getSamplerType(texFmt);
    sampleParams.colorBias               = fmtInfo.lookupBias;
    sampleParams.colorScale              = fmtInfo.lookupScale;
    sampleParams.lodMode                 = LODMODE_EXACT;

    m_testCtx.getLog() << TestLog::Message << "Coordinates: " << curCase.bottomLeft << " -> " << curCase.topRight
                       << TestLog::EndMessage;

    for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; faceNdx++)
    {
        const tcu::CubeFace face = tcu::CubeFace(faceNdx);
        tcu::Surface result(viewport.width, viewport.height);
        vector<float> texCoord;

        computeQuadTexCoordCube(texCoord, face, curCase.bottomLeft, curCase.topRight);

        m_testCtx.getLog() << TestLog::Message << "Face " << getFaceDesc(face) << TestLog::EndMessage;

        // \todo Log texture coordinates.

        m_renderer.renderQuad(0, &texCoord[0], sampleParams);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Draw");

        glu::readPixels(m_renderCtx, viewport.x, viewport.y, result.getAccess());
        GLU_EXPECT_NO_ERROR(gl.getError(), "Read pixels");

        {
            const bool isNearestOnly           = m_minFilter == GL_NEAREST && m_magFilter == GL_NEAREST;
            const tcu::PixelFormat pixelFormat = m_renderCtx.getRenderTarget().getPixelFormat();
            const tcu::IVec4 colorBits         = max(getBitsVec(pixelFormat) - (isNearestOnly ? 1 : 2),
                                                     tcu::IVec4(0)); // 1 inaccurate bit if nearest only, 2 otherwise
            tcu::LodPrecision lodPrecision;
            tcu::LookupPrecision lookupPrecision;

            lodPrecision.derivateBits      = 5;
            lodPrecision.lodBits           = 3;
            lookupPrecision.colorThreshold = tcu::computeFixedPointThreshold(colorBits) / sampleParams.colorScale;
            lookupPrecision.coordBits      = tcu::IVec3(9, 9, 9); // mediump interpolation
            lookupPrecision.uvwBits        = tcu::IVec3(3, 3, 0);
            lookupPrecision.colorMask      = getCompareMask(pixelFormat);

            const bool isOk =
                verifyTextureResult(m_testCtx, result.getAccess(), curCase.texture->getRefTexture(), &texCoord[0],
                                    sampleParams, lookupPrecision, lodPrecision, pixelFormat);

            if (!isOk)
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image verification failed");
        }
    }

    m_caseNdx += 1;
    return m_caseNdx < (int)m_cases.size() ? CONTINUE : STOP;
}

TextureFilteringTests::TextureFilteringTests(Context &context)
    : TestCaseGroup(context, "filtering", "Texture Filtering Tests")
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
    } formats[] = {{"rgba8888", GL_RGBA, GL_UNSIGNED_BYTE},
                   {"rgb888", GL_RGB, GL_UNSIGNED_BYTE},
                   {"rgba4444", GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},
                   {"l8", GL_LUMINANCE, GL_UNSIGNED_BYTE}};

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

    // 2D ETC1 texture cases.
    {
        std::vector<std::string> filenames;
        for (int i = 0; i <= 7; i++)
            filenames.push_back(string("data/etc1/photo_helsinki_mip_") + de::toString(i) + ".pkm");

        FOR_EACH(minFilter, minFilterModes,
                 FOR_EACH(magFilter, magFilterModes, FOR_EACH(wrapMode, wrapModes, {
                              string name = string("") + minFilterModes[minFilter].name + "_" +
                                            magFilterModes[magFilter].name + "_" + wrapModes[wrapMode].name + "_etc1";

                              group2D->addChild(new Texture2DFilteringCase(
                                  m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.c_str(), "",
                                  minFilterModes[minFilter].mode, magFilterModes[magFilter].mode,
                                  wrapModes[wrapMode].mode, wrapModes[wrapMode].mode, filenames));
                          })))
    }

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

    // Cubemap ETC1 cases
    {
        static const char *faceExt[] = {"neg_x", "pos_x", "neg_y", "pos_y", "neg_z", "pos_z"};

        const int numLevels = 7;
        vector<string> filenames;
        for (int level = 0; level < numLevels; level++)
            for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
                filenames.push_back(string("data/etc1/skybox_") + faceExt[face] + "_mip_" + de::toString(level) +
                                    ".pkm");

        FOR_EACH(minFilter, minFilterModes, FOR_EACH(magFilter, magFilterModes, {
                     string name = string("") + minFilterModes[minFilter].name + "_" + magFilterModes[magFilter].name +
                                   "_clamp_etc1";

                     groupCube->addChild(new TextureCubeFilteringCase(
                         m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.c_str(), "",
                         minFilterModes[minFilter].mode, magFilterModes[magFilter].mode, GL_CLAMP_TO_EDGE,
                         GL_CLAMP_TO_EDGE, filenames));
                 }))
    }
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
