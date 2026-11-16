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
 * \brief Texture filtering tests.
 *//*--------------------------------------------------------------------*/

#include "es3fTextureFilteringTests.hpp"
#include "glsTextureTestUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluTexture.hpp"
#include "gluTextureUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTexLookupVerifier.hpp"
#include "tcuVectorUtil.hpp"
#include "deStringUtil.hpp"
#include "deString.h"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "gluContextInfo.hpp"
#include "deUniquePtr.hpp"

using de::MovePtr;
using glu::ContextInfo;

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::string;
using std::vector;
using tcu::TestLog;
using namespace gls::TextureTestUtil;
using namespace glu::TextureTestUtil;

enum
{
    TEX2D_VIEWPORT_WIDTH      = 64,
    TEX2D_VIEWPORT_HEIGHT     = 64,
    TEX2D_MIN_VIEWPORT_WIDTH  = 64,
    TEX2D_MIN_VIEWPORT_HEIGHT = 64,

    TEX3D_VIEWPORT_WIDTH      = 64,
    TEX3D_VIEWPORT_HEIGHT     = 64,
    TEX3D_MIN_VIEWPORT_WIDTH  = 64,
    TEX3D_MIN_VIEWPORT_HEIGHT = 64
};

namespace
{

void checkSupport(const glu::ContextInfo &info, uint32_t internalFormat)
{
    if (internalFormat == GL_SR8_EXT && !info.isExtensionSupported("GL_EXT_texture_sRGB_R8"))
        TCU_THROW(NotSupportedError, "GL_EXT_texture_sRGB_R8 is not supported.");

    if (internalFormat == GL_SRG8_EXT && !info.isExtensionSupported("GL_EXT_texture_sRGB_RG8"))
        TCU_THROW(NotSupportedError, "GL_EXT_texture_sRGB_RG8 is not supported.");

    if ((internalFormat == GL_BGRA || internalFormat == GL_BGRA8_EXT) &&
        !info.isExtensionSupported("GL_EXT_texture_format_BGRA8888"))
        TCU_THROW(NotSupportedError, "GL_EXT_texture_format_BGRA8888 is not supported.");
}

} // namespace

class Texture2DFilteringCase : public tcu::TestCase
{
public:
    Texture2DFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                           const char *name, const char *desc, uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                           uint32_t wrapT, uint32_t internalFormat, int width, int height);
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

    const uint32_t m_internalFormat;
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
                                               uint32_t internalFormat, int width, int height)
    : TestCase(testCtx, name, desc)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(ctxInfo)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_internalFormat(internalFormat)
    , m_width(width)
    , m_height(height)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
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
    , m_internalFormat(GL_NONE)
    , m_width(0)
    , m_height(0)
    , m_filenames(filenames)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
    , m_caseNdx(0)
{
}

Texture2DFilteringCase::~Texture2DFilteringCase(void)
{
    deinit();
}

void Texture2DFilteringCase::init(void)
{
    checkSupport(m_renderCtxInfo, m_internalFormat);

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
                m_textures.push_back(new glu::Texture2D(m_renderCtx, m_internalFormat, m_width, m_height));

            const bool mipmaps  = true;
            const int numLevels = mipmaps ? deLog2Floor32(de::max(m_width, m_height)) + 1 : 1;
            const tcu::TextureFormatInfo fmtInfo =
                tcu::getTextureFormatInfo(m_textures[0]->getRefTexture().getFormat());
            const tcu::Vec4 cBias  = fmtInfo.valueMin;
            const tcu::Vec4 cScale = fmtInfo.valueMax - fmtInfo.valueMin;

            // Fill first gradient texture.
            for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
            {
                tcu::Vec4 gMin = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias;
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

            const float viewportW = (float)de::min<int>(TEX2D_VIEWPORT_WIDTH, m_renderCtx.getRenderTarget().getWidth());
            const float viewportH =
                (float)de::min<int>(TEX2D_VIEWPORT_HEIGHT, m_renderCtx.getRenderTarget().getHeight());

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
    const RandomViewport viewport(m_renderCtx.getRenderTarget(), TEX2D_VIEWPORT_WIDTH, TEX2D_VIEWPORT_HEIGHT,
                                  deStringHash(getName()) ^ deInt32Hash(m_caseNdx));
    const tcu::TextureFormat texFmt      = m_textures[0]->getRefTexture().getFormat();
    const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);
    const FilterCase &curCase            = m_cases[m_caseNdx];
    const tcu::ScopedLogSection section(m_testCtx.getLog(), string("Test") + de::toString(m_caseNdx),
                                        string("Test ") + de::toString(m_caseNdx));
    ReferenceParams refParams(TEXTURETYPE_2D);
    tcu::Surface rendered(viewport.width, viewport.height);
    vector<float> texCoord;

    if (viewport.width < TEX2D_MIN_VIEWPORT_WIDTH || viewport.height < TEX2D_MIN_VIEWPORT_HEIGHT)
        throw tcu::NotSupportedError("Too small render target", "", __FILE__, __LINE__);

    // Setup params for reference.
    refParams.sampler     = glu::mapGLSampler(m_wrapS, m_wrapT, m_minFilter, m_magFilter);
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

        lodPrecision.derivateBits      = 18;
        lodPrecision.lodBits           = 6;
        lookupPrecision.colorThreshold = tcu::computeFixedPointThreshold(colorBits) / refParams.colorScale;
        lookupPrecision.coordBits      = tcu::IVec3(20, 20, 0);
        lookupPrecision.uvwBits        = tcu::IVec3(7, 7, 0);
        lookupPrecision.colorMask      = getCompareMask(pixelFormat);

        const bool isHighQuality =
            verifyTextureResult(m_testCtx, rendered.getAccess(), curCase.texture->getRefTexture(), &texCoord[0],
                                refParams, lookupPrecision, lodPrecision, pixelFormat);

        if (!isHighQuality)
        {
            // Evaluate against lower precision requirements.
            lodPrecision.lodBits    = 4;
            lookupPrecision.uvwBits = tcu::IVec3(4, 4, 0);

            m_testCtx.getLog()
                << TestLog::Message
                << "Warning: Verification against high precision requirements failed, trying with lower requirements."
                << TestLog::EndMessage;

            const bool isOk = verifyTextureResult(m_testCtx, rendered.getAccess(), curCase.texture->getRefTexture(),
                                                  &texCoord[0], refParams, lookupPrecision, lodPrecision, pixelFormat);

            if (!isOk)
            {
                m_testCtx.getLog()
                    << TestLog::Message
                    << "ERROR: Verification against low precision requirements failed, failing test case."
                    << TestLog::EndMessage;
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image verification failed");
            }
            else if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING, "Low-quality filtering result");
        }
    }

    m_caseNdx += 1;
    return m_caseNdx < (int)m_cases.size() ? CONTINUE : STOP;
}

class TextureCubeFilteringCase : public tcu::TestCase
{
public:
    TextureCubeFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                             const char *name, const char *desc, uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                             uint32_t wrapT, bool onlySampleFaceInterior, uint32_t internalFormat, int width,
                             int height);
    TextureCubeFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                             const char *name, const char *desc, uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                             uint32_t wrapT, bool onlySampleFaceInterior, const std::vector<std::string> &filenames);
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
    const bool m_onlySampleFaceInterior; //!< If true, we avoid sampling anywhere near a face's edges.

    const uint32_t m_internalFormat;
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
                                                   uint32_t wrapT, bool onlySampleFaceInterior, uint32_t internalFormat,
                                                   int width, int height)
    : TestCase(testCtx, name, desc)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(ctxInfo)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_onlySampleFaceInterior(onlySampleFaceInterior)
    , m_internalFormat(internalFormat)
    , m_width(width)
    , m_height(height)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
    , m_caseNdx(0)
{
}

TextureCubeFilteringCase::TextureCubeFilteringCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                   const glu::ContextInfo &ctxInfo, const char *name, const char *desc,
                                                   uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                                                   uint32_t wrapT, bool onlySampleFaceInterior,
                                                   const std::vector<std::string> &filenames)
    : TestCase(testCtx, name, desc)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(ctxInfo)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_onlySampleFaceInterior(onlySampleFaceInterior)
    , m_internalFormat(GL_NONE)
    , m_width(0)
    , m_height(0)
    , m_filenames(filenames)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
    , m_caseNdx(0)
{
}

TextureCubeFilteringCase::~TextureCubeFilteringCase(void)
{
    deinit();
}

void TextureCubeFilteringCase::init(void)
{
    checkSupport(m_renderCtxInfo, m_internalFormat);

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
                m_textures.push_back(new glu::TextureCube(m_renderCtx, m_internalFormat, m_width));

            const int numLevels            = deLog2Floor32(de::max(m_width, m_height)) + 1;
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

            if (m_onlySampleFaceInterior)
            {
                m_cases.push_back(FilterCase(tex0, tcu::Vec2(-0.8f, -0.8f), tcu::Vec2(0.8f, 0.8f))); // minification
                m_cases.push_back(FilterCase(tex0, tcu::Vec2(0.5f, 0.65f), tcu::Vec2(0.8f, 0.8f)));  // magnification
                m_cases.push_back(FilterCase(tex1, tcu::Vec2(-0.8f, -0.8f), tcu::Vec2(0.8f, 0.8f))); // minification
                m_cases.push_back(FilterCase(tex1, tcu::Vec2(0.2f, 0.2f), tcu::Vec2(0.6f, 0.5f)));   // magnification
            }
            else
            {
                if (m_renderCtx.getRenderTarget().getNumSamples() == 0)
                    m_cases.push_back(
                        FilterCase(tex0, tcu::Vec2(-1.25f, -1.2f), tcu::Vec2(1.2f, 1.25f))); // minification
                else
                    m_cases.push_back(FilterCase(
                        tex0, tcu::Vec2(-1.19f, -1.3f),
                        tcu::Vec2(
                            1.1f,
                            1.35f))); // minification - w/ tweak to avoid hitting triangle edges with face switchpoint

                m_cases.push_back(FilterCase(tex0, tcu::Vec2(0.8f, 0.8f), tcu::Vec2(1.25f, 1.20f)));   // magnification
                m_cases.push_back(FilterCase(tex1, tcu::Vec2(-1.19f, -1.3f), tcu::Vec2(1.1f, 1.35f))); // minification
                m_cases.push_back(FilterCase(tex1, tcu::Vec2(-1.2f, -1.1f), tcu::Vec2(-0.8f, -0.8f))); // magnification
            }
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
    sampleParams.sampler = glu::mapGLSampler(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, m_minFilter, m_magFilter);
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

            lodPrecision.derivateBits      = 10;
            lodPrecision.lodBits           = 5;
            lookupPrecision.colorThreshold = tcu::computeFixedPointThreshold(colorBits) / sampleParams.colorScale;
            lookupPrecision.coordBits      = tcu::IVec3(10, 10, 10);
            lookupPrecision.uvwBits        = tcu::IVec3(6, 6, 0);
            lookupPrecision.colorMask      = getCompareMask(pixelFormat);

            const bool isHighQuality =
                verifyTextureResult(m_testCtx, result.getAccess(), curCase.texture->getRefTexture(), &texCoord[0],
                                    sampleParams, lookupPrecision, lodPrecision, pixelFormat);

            if (!isHighQuality)
            {
                // Evaluate against lower precision requirements.
                lodPrecision.lodBits    = 4;
                lookupPrecision.uvwBits = tcu::IVec3(4, 4, 0);

                m_testCtx.getLog() << TestLog::Message
                                   << "Warning: Verification against high precision requirements failed, trying with "
                                      "lower requirements."
                                   << TestLog::EndMessage;

                const bool isOk =
                    verifyTextureResult(m_testCtx, result.getAccess(), curCase.texture->getRefTexture(), &texCoord[0],
                                        sampleParams, lookupPrecision, lodPrecision, pixelFormat);

                if (!isOk)
                {
                    m_testCtx.getLog()
                        << TestLog::Message
                        << "ERROR: Verification against low precision requirements failed, failing test case."
                        << TestLog::EndMessage;
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image verification failed");
                }
                else if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING, "Low-quality filtering result");
            }
        }
    }

    m_caseNdx += 1;
    return m_caseNdx < (int)m_cases.size() ? CONTINUE : STOP;
}

// 2D array filtering

class Texture2DArrayFilteringCase : public TestCase
{
public:
    Texture2DArrayFilteringCase(Context &context, const char *name, const char *desc, uint32_t minFilter,
                                uint32_t magFilter, uint32_t wrapS, uint32_t wrapT, uint32_t internalFormat, int width,
                                int height, int numLayers);
    ~Texture2DArrayFilteringCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture2DArrayFilteringCase(const Texture2DArrayFilteringCase &);
    Texture2DArrayFilteringCase &operator=(const Texture2DArrayFilteringCase &);

    const uint32_t m_minFilter;
    const uint32_t m_magFilter;
    const uint32_t m_wrapS;
    const uint32_t m_wrapT;

    const uint32_t m_internalFormat;
    const int m_width;
    const int m_height;
    const int m_numLayers;

    struct FilterCase
    {
        const glu::Texture2DArray *texture;
        tcu::Vec2 lod;
        tcu::Vec2 offset;
        tcu::Vec2 layerRange;

        FilterCase(void) : texture(nullptr)
        {
        }

        FilterCase(const glu::Texture2DArray *tex_, const tcu::Vec2 &lod_, const tcu::Vec2 &offset_,
                   const tcu::Vec2 &layerRange_)
            : texture(tex_)
            , lod(lod_)
            , offset(offset_)
            , layerRange(layerRange_)
        {
        }
    };

    glu::Texture2DArray *m_gradientTex;
    glu::Texture2DArray *m_gridTex;

    TextureRenderer m_renderer;

    std::vector<FilterCase> m_cases;
    int m_caseNdx;
};

Texture2DArrayFilteringCase::Texture2DArrayFilteringCase(Context &context, const char *name, const char *desc,
                                                         uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                                                         uint32_t wrapT, uint32_t internalFormat, int width, int height,
                                                         int numLayers)
    : TestCase(context, name, desc)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_internalFormat(internalFormat)
    , m_width(width)
    , m_height(height)
    , m_numLayers(numLayers)
    , m_gradientTex(nullptr)
    , m_gridTex(nullptr)
    , m_renderer(context.getRenderContext(), context.getTestContext().getLog(), glu::GLSL_VERSION_300_ES,
                 glu::PRECISION_HIGHP)
    , m_caseNdx(0)
{
}

Texture2DArrayFilteringCase::~Texture2DArrayFilteringCase(void)
{
    Texture2DArrayFilteringCase::deinit();
}

void Texture2DArrayFilteringCase::init(void)
{
    checkSupport(m_context.getContextInfo(), m_internalFormat);

    try
    {
        const tcu::TextureFormat texFmt      = glu::mapGLInternalFormat(m_internalFormat);
        const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);
        const tcu::Vec4 cScale               = fmtInfo.valueMax - fmtInfo.valueMin;
        const tcu::Vec4 cBias                = fmtInfo.valueMin;
        const int numLevels                  = deLog2Floor32(de::max(m_width, m_height)) + 1;

        // Create textures.
        m_gradientTex =
            new glu::Texture2DArray(m_context.getRenderContext(), m_internalFormat, m_width, m_height, m_numLayers);
        m_gridTex =
            new glu::Texture2DArray(m_context.getRenderContext(), m_internalFormat, m_width, m_height, m_numLayers);

        const tcu::IVec4 levelSwz[] = {
            tcu::IVec4(0, 1, 2, 3),
            tcu::IVec4(2, 1, 3, 0),
            tcu::IVec4(3, 0, 1, 2),
            tcu::IVec4(1, 3, 2, 0),
        };

        // Fill first gradient texture (gradient direction varies between layers).
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            m_gradientTex->getRefTexture().allocLevel(levelNdx);

            const tcu::PixelBufferAccess levelBuf = m_gradientTex->getRefTexture().getLevel(levelNdx);

            for (int layerNdx = 0; layerNdx < m_numLayers; layerNdx++)
            {
                const tcu::IVec4 swz = levelSwz[layerNdx % DE_LENGTH_OF_ARRAY(levelSwz)];
                const tcu::Vec4 gMin =
                    tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f).swizzle(swz[0], swz[1], swz[2], swz[3]) * cScale + cBias;
                const tcu::Vec4 gMax =
                    tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f).swizzle(swz[0], swz[1], swz[2], swz[3]) * cScale + cBias;

                tcu::fillWithComponentGradients(
                    tcu::getSubregion(levelBuf, 0, 0, layerNdx, levelBuf.getWidth(), levelBuf.getHeight(), 1), gMin,
                    gMax);
            }
        }

        // Fill second with grid texture (each layer has unique colors).
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            m_gridTex->getRefTexture().allocLevel(levelNdx);

            const tcu::PixelBufferAccess levelBuf = m_gridTex->getRefTexture().getLevel(levelNdx);

            for (int layerNdx = 0; layerNdx < m_numLayers; layerNdx++)
            {
                const uint32_t step   = 0x00ffffff / (numLevels * m_numLayers - 1);
                const uint32_t rgb    = step * (levelNdx + layerNdx * numLevels);
                const uint32_t colorA = 0xff000000 | rgb;
                const uint32_t colorB = 0xff000000 | ~rgb;

                tcu::fillWithGrid(
                    tcu::getSubregion(levelBuf, 0, 0, layerNdx, levelBuf.getWidth(), levelBuf.getHeight(), 1), 4,
                    tcu::RGBA(colorA).toVec() * cScale + cBias, tcu::RGBA(colorB).toVec() * cScale + cBias);
            }
        }

        // Upload.
        m_gradientTex->upload();
        m_gridTex->upload();

        // Test cases
        m_cases.push_back(FilterCase(m_gradientTex, tcu::Vec2(1.5f, 2.8f), tcu::Vec2(-1.0f, -2.7f),
                                     tcu::Vec2(-0.5f, float(m_numLayers) + 0.5f)));
        m_cases.push_back(FilterCase(m_gridTex, tcu::Vec2(0.2f, 0.175f), tcu::Vec2(-2.0f, -3.7f),
                                     tcu::Vec2(-0.5f, float(m_numLayers) + 0.5f)));
        m_cases.push_back(FilterCase(m_gridTex, tcu::Vec2(-0.8f, -2.3f), tcu::Vec2(0.2f, -0.1f),
                                     tcu::Vec2(float(m_numLayers) + 0.5f, -0.5f)));

        // Level rounding - only in single-sample configs as multisample configs may produce smooth transition at the middle.
        if (m_context.getRenderTarget().getNumSamples() == 0)
            m_cases.push_back(FilterCase(m_gradientTex, tcu::Vec2(-2.0f, -1.5f), tcu::Vec2(-0.1f, 0.9f),
                                         tcu::Vec2(1.50001f, 1.49999f)));

        m_caseNdx = 0;
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }
    catch (...)
    {
        // Clean up to save memory.
        Texture2DArrayFilteringCase::deinit();
        throw;
    }
}

void Texture2DArrayFilteringCase::deinit(void)
{
    delete m_gradientTex;
    delete m_gridTex;

    m_gradientTex = nullptr;
    m_gridTex     = nullptr;

    m_renderer.clear();
    m_cases.clear();
}

Texture2DArrayFilteringCase::IterateResult Texture2DArrayFilteringCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const RandomViewport viewport(m_context.getRenderTarget(), TEX3D_VIEWPORT_WIDTH, TEX3D_VIEWPORT_HEIGHT,
                                  deStringHash(getName()) ^ deInt32Hash(m_caseNdx));
    const FilterCase &curCase            = m_cases[m_caseNdx];
    const tcu::TextureFormat texFmt      = curCase.texture->getRefTexture().getFormat();
    const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);
    const tcu::ScopedLogSection section(m_testCtx.getLog(), string("Test") + de::toString(m_caseNdx),
                                        string("Test ") + de::toString(m_caseNdx));
    ReferenceParams refParams(TEXTURETYPE_2D_ARRAY);
    tcu::Surface rendered(viewport.width, viewport.height);
    tcu::Vec3 texCoord[4];

    if (viewport.width < TEX3D_MIN_VIEWPORT_WIDTH || viewport.height < TEX3D_MIN_VIEWPORT_HEIGHT)
        throw tcu::NotSupportedError("Too small render target", "", __FILE__, __LINE__);

    // Setup params for reference.
    refParams.sampler     = glu::mapGLSampler(m_wrapS, m_wrapT, m_wrapT, m_minFilter, m_magFilter);
    refParams.samplerType = getSamplerType(texFmt);
    refParams.lodMode     = LODMODE_EXACT;
    refParams.colorBias   = fmtInfo.lookupBias;
    refParams.colorScale  = fmtInfo.lookupScale;

    // Compute texture coordinates.
    m_testCtx.getLog() << TestLog::Message << "Approximate lod per axis = " << curCase.lod
                       << ", offset = " << curCase.offset << TestLog::EndMessage;

    {
        const float lodX = curCase.lod.x();
        const float lodY = curCase.lod.y();
        const float oX   = curCase.offset.x();
        const float oY   = curCase.offset.y();
        const float sX   = deFloatExp2(lodX) * float(viewport.width) / float(m_gradientTex->getRefTexture().getWidth());
        const float sY = deFloatExp2(lodY) * float(viewport.height) / float(m_gradientTex->getRefTexture().getHeight());
        const float l0 = curCase.layerRange.x();
        const float l1 = curCase.layerRange.y();

        texCoord[0] = tcu::Vec3(oX, oY, l0);
        texCoord[1] = tcu::Vec3(oX, oY + sY, l0 * 0.5f + l1 * 0.5f);
        texCoord[2] = tcu::Vec3(oX + sX, oY, l0 * 0.5f + l1 * 0.5f);
        texCoord[3] = tcu::Vec3(oX + sX, oY + sY, l1);
    }

    gl.bindTexture(GL_TEXTURE_2D_ARRAY, curCase.texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, m_magFilter);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, m_wrapS);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, m_wrapT);

    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);
    m_renderer.renderQuad(0, (const float *)&texCoord[0], refParams);
    glu::readPixels(m_context.getRenderContext(), viewport.x, viewport.y, rendered.getAccess());

    {
        const bool isNearestOnly           = m_minFilter == GL_NEAREST && m_magFilter == GL_NEAREST;
        const tcu::PixelFormat pixelFormat = m_context.getRenderTarget().getPixelFormat();
        const tcu::IVec4 colorBits         = max(getBitsVec(pixelFormat) - (isNearestOnly ? 1 : 2),
                                                 tcu::IVec4(0)); // 1 inaccurate bit if nearest only, 2 otherwise
        tcu::LodPrecision lodPrecision;
        tcu::LookupPrecision lookupPrecision;

        lodPrecision.derivateBits      = 18;
        lodPrecision.lodBits           = 6;
        lookupPrecision.colorThreshold = tcu::computeFixedPointThreshold(colorBits) / refParams.colorScale;
        lookupPrecision.coordBits      = tcu::IVec3(20, 20, 20);
        lookupPrecision.uvwBits        = tcu::IVec3(7, 7, 0);
        lookupPrecision.colorMask      = getCompareMask(pixelFormat);

        const bool isHighQuality =
            verifyTextureResult(m_testCtx, rendered.getAccess(), curCase.texture->getRefTexture(),
                                (const float *)&texCoord[0], refParams, lookupPrecision, lodPrecision, pixelFormat);

        if (!isHighQuality)
        {
            // Evaluate against lower precision requirements.
            lodPrecision.lodBits    = 4;
            lookupPrecision.uvwBits = tcu::IVec3(4, 4, 0);

            m_testCtx.getLog()
                << TestLog::Message
                << "Warning: Verification against high precision requirements failed, trying with lower requirements."
                << TestLog::EndMessage;

            const bool isOk =
                verifyTextureResult(m_testCtx, rendered.getAccess(), curCase.texture->getRefTexture(),
                                    (const float *)&texCoord[0], refParams, lookupPrecision, lodPrecision, pixelFormat);

            if (!isOk)
            {
                m_testCtx.getLog()
                    << TestLog::Message
                    << "ERROR: Verification against low precision requirements failed, failing test case."
                    << TestLog::EndMessage;
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image verification failed");
            }
            else if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING, "Low-quality filtering result");
        }
    }

    m_caseNdx += 1;
    return m_caseNdx < (int)m_cases.size() ? CONTINUE : STOP;
}

// 3D filtering

class Texture3DFilteringCase : public TestCase
{
public:
    Texture3DFilteringCase(Context &context, const char *name, const char *desc, uint32_t minFilter, uint32_t magFilter,
                           uint32_t wrapS, uint32_t wrapT, uint32_t wrapR, uint32_t internalFormat, int width,
                           int height, int depth);
    ~Texture3DFilteringCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture3DFilteringCase(const Texture3DFilteringCase &other);
    Texture3DFilteringCase &operator=(const Texture3DFilteringCase &other);

    const uint32_t m_minFilter;
    const uint32_t m_magFilter;
    const uint32_t m_wrapS;
    const uint32_t m_wrapT;
    const uint32_t m_wrapR;

    const uint32_t m_internalFormat;
    const int m_width;
    const int m_height;
    const int m_depth;

    struct FilterCase
    {
        const glu::Texture3D *texture;
        tcu::Vec3 lod;
        tcu::Vec3 offset;

        FilterCase(void) : texture(nullptr)
        {
        }

        FilterCase(const glu::Texture3D *tex_, const tcu::Vec3 &lod_, const tcu::Vec3 &offset_)
            : texture(tex_)
            , lod(lod_)
            , offset(offset_)
        {
        }
    };

    glu::Texture3D *m_gradientTex;
    glu::Texture3D *m_gridTex;

    TextureRenderer m_renderer;

    std::vector<FilterCase> m_cases;
    int m_caseNdx;
};

Texture3DFilteringCase::Texture3DFilteringCase(Context &context, const char *name, const char *desc, uint32_t minFilter,
                                               uint32_t magFilter, uint32_t wrapS, uint32_t wrapT, uint32_t wrapR,
                                               uint32_t internalFormat, int width, int height, int depth)
    : TestCase(context, name, desc)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_wrapR(wrapR)
    , m_internalFormat(internalFormat)
    , m_width(width)
    , m_height(height)
    , m_depth(depth)
    , m_gradientTex(nullptr)
    , m_gridTex(nullptr)
    , m_renderer(context.getRenderContext(), context.getTestContext().getLog(), glu::GLSL_VERSION_300_ES,
                 glu::PRECISION_HIGHP)
    , m_caseNdx(0)
{
}

Texture3DFilteringCase::~Texture3DFilteringCase(void)
{
    Texture3DFilteringCase::deinit();
}

void Texture3DFilteringCase::init(void)
{
    checkSupport(m_context.getContextInfo(), m_internalFormat);

    try
    {
        const tcu::TextureFormat texFmt      = glu::mapGLInternalFormat(m_internalFormat);
        const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);
        const tcu::Vec4 cScale               = fmtInfo.valueMax - fmtInfo.valueMin;
        const tcu::Vec4 cBias                = fmtInfo.valueMin;
        const int numLevels                  = deLog2Floor32(de::max(de::max(m_width, m_height), m_depth)) + 1;

        // Create textures.
        m_gradientTex = new glu::Texture3D(m_context.getRenderContext(), m_internalFormat, m_width, m_height, m_depth);
        m_gridTex     = new glu::Texture3D(m_context.getRenderContext(), m_internalFormat, m_width, m_height, m_depth);

        // Fill first gradient texture.
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            tcu::Vec4 gMin = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias;
            tcu::Vec4 gMax = tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f) * cScale + cBias;

            m_gradientTex->getRefTexture().allocLevel(levelNdx);
            tcu::fillWithComponentGradients(m_gradientTex->getRefTexture().getLevel(levelNdx), gMin, gMax);
        }

        // Fill second with grid texture.
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            uint32_t step   = 0x00ffffff / numLevels;
            uint32_t rgb    = step * levelNdx;
            uint32_t colorA = 0xff000000 | rgb;
            uint32_t colorB = 0xff000000 | ~rgb;

            m_gridTex->getRefTexture().allocLevel(levelNdx);
            tcu::fillWithGrid(m_gridTex->getRefTexture().getLevel(levelNdx), 4,
                              tcu::RGBA(colorA).toVec() * cScale + cBias, tcu::RGBA(colorB).toVec() * cScale + cBias);
        }

        // Upload.
        m_gradientTex->upload();
        m_gridTex->upload();

        // Test cases
        m_cases.push_back(FilterCase(m_gradientTex, tcu::Vec3(1.5f, 2.8f, 1.0f), tcu::Vec3(-1.0f, -2.7f, -2.275f)));
        m_cases.push_back(FilterCase(m_gradientTex, tcu::Vec3(-2.0f, -1.5f, -1.8f), tcu::Vec3(-0.1f, 0.9f, -0.25f)));
        m_cases.push_back(FilterCase(m_gridTex, tcu::Vec3(0.2f, 0.175f, 0.3f), tcu::Vec3(-2.0f, -3.7f, -1.825f)));
        m_cases.push_back(FilterCase(m_gridTex, tcu::Vec3(-0.8f, -2.3f, -2.5f), tcu::Vec3(0.2f, -0.1f, 1.325f)));

        m_caseNdx = 0;
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }
    catch (...)
    {
        // Clean up to save memory.
        Texture3DFilteringCase::deinit();
        throw;
    }
}

void Texture3DFilteringCase::deinit(void)
{
    delete m_gradientTex;
    delete m_gridTex;

    m_gradientTex = nullptr;
    m_gridTex     = nullptr;

    m_renderer.clear();
    m_cases.clear();
}

Texture3DFilteringCase::IterateResult Texture3DFilteringCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const RandomViewport viewport(m_context.getRenderTarget(), TEX3D_VIEWPORT_WIDTH, TEX3D_VIEWPORT_HEIGHT,
                                  deStringHash(getName()) ^ deInt32Hash(m_caseNdx));
    const FilterCase &curCase            = m_cases[m_caseNdx];
    const tcu::TextureFormat texFmt      = curCase.texture->getRefTexture().getFormat();
    const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);
    const tcu::ScopedLogSection section(m_testCtx.getLog(), string("Test") + de::toString(m_caseNdx),
                                        string("Test ") + de::toString(m_caseNdx));
    ReferenceParams refParams(TEXTURETYPE_3D);
    tcu::Surface rendered(viewport.width, viewport.height);
    tcu::Vec3 texCoord[4];

    if (viewport.width < TEX3D_MIN_VIEWPORT_WIDTH || viewport.height < TEX3D_MIN_VIEWPORT_HEIGHT)
        throw tcu::NotSupportedError("Too small render target", "", __FILE__, __LINE__);

    // Setup params for reference.
    refParams.sampler     = glu::mapGLSampler(m_wrapS, m_wrapT, m_wrapR, m_minFilter, m_magFilter);
    refParams.samplerType = getSamplerType(texFmt);
    refParams.lodMode     = LODMODE_EXACT;
    refParams.colorBias   = fmtInfo.lookupBias;
    refParams.colorScale  = fmtInfo.lookupScale;

    // Compute texture coordinates.
    m_testCtx.getLog() << TestLog::Message << "Approximate lod per axis = " << curCase.lod
                       << ", offset = " << curCase.offset << TestLog::EndMessage;

    {
        const float lodX = curCase.lod.x();
        const float lodY = curCase.lod.y();
        const float lodZ = curCase.lod.z();
        const float oX   = curCase.offset.x();
        const float oY   = curCase.offset.y();
        const float oZ   = curCase.offset.z();
        const float sX   = deFloatExp2(lodX) * float(viewport.width) / float(m_gradientTex->getRefTexture().getWidth());
        const float sY = deFloatExp2(lodY) * float(viewport.height) / float(m_gradientTex->getRefTexture().getHeight());
        const float sZ = deFloatExp2(lodZ) * float(de::max(viewport.width, viewport.height)) /
                         float(m_gradientTex->getRefTexture().getDepth());

        texCoord[0] = tcu::Vec3(oX, oY, oZ);
        texCoord[1] = tcu::Vec3(oX, oY + sY, oZ + sZ * 0.5f);
        texCoord[2] = tcu::Vec3(oX + sX, oY, oZ + sZ * 0.5f);
        texCoord[3] = tcu::Vec3(oX + sX, oY + sY, oZ + sZ);
    }

    gl.bindTexture(GL_TEXTURE_3D, curCase.texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, m_magFilter);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, m_wrapS);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, m_wrapT);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, m_wrapR);

    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);
    m_renderer.renderQuad(0, (const float *)&texCoord[0], refParams);
    glu::readPixels(m_context.getRenderContext(), viewport.x, viewport.y, rendered.getAccess());

    {
        const bool isNearestOnly           = m_minFilter == GL_NEAREST && m_magFilter == GL_NEAREST;
        const tcu::PixelFormat pixelFormat = m_context.getRenderTarget().getPixelFormat();
        const tcu::IVec4 colorBits         = max(getBitsVec(pixelFormat) - (isNearestOnly ? 1 : 2),
                                                 tcu::IVec4(0)); // 1 inaccurate bit if nearest only, 2 otherwise
        tcu::LodPrecision lodPrecision;
        tcu::LookupPrecision lookupPrecision;

        lodPrecision.derivateBits      = 18;
        lodPrecision.lodBits           = 6;
        lookupPrecision.colorThreshold = tcu::computeFixedPointThreshold(colorBits) / refParams.colorScale;
        lookupPrecision.coordBits      = tcu::IVec3(20, 20, 20);
        lookupPrecision.uvwBits        = tcu::IVec3(7, 7, 7);
        lookupPrecision.colorMask      = getCompareMask(pixelFormat);

        const bool isHighQuality =
            verifyTextureResult(m_testCtx, rendered.getAccess(), curCase.texture->getRefTexture(),
                                (const float *)&texCoord[0], refParams, lookupPrecision, lodPrecision, pixelFormat);

        if (!isHighQuality)
        {
            // Evaluate against lower precision requirements.
            lodPrecision.lodBits    = 4;
            lookupPrecision.uvwBits = tcu::IVec3(4, 4, 4);

            m_testCtx.getLog()
                << TestLog::Message
                << "Warning: Verification against high precision requirements failed, trying with lower requirements."
                << TestLog::EndMessage;

            const bool isOk =
                verifyTextureResult(m_testCtx, rendered.getAccess(), curCase.texture->getRefTexture(),
                                    (const float *)&texCoord[0], refParams, lookupPrecision, lodPrecision, pixelFormat);

            if (!isOk)
            {
                m_testCtx.getLog()
                    << TestLog::Message
                    << "ERROR: Verification against low precision requirements failed, failing test case."
                    << TestLog::EndMessage;
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image verification failed");
            }
            else if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING, "Low-quality filtering result");
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
        int width;
        int height;
    } sizes2D[] = {{4, 8}, {32, 64}, {128, 128}, {3, 7}, {31, 55}, {127, 99}};

    static const struct
    {
        int width;
        int height;
    } sizesCube[] = {{8, 8}, {64, 64}, {128, 128}, {7, 7}, {63, 63}};

    static const struct
    {
        int width;
        int height;
        int numLayers;
    } sizes2DArray[] = {{4, 8, 8}, {32, 64, 16}, {128, 32, 64}, {3, 7, 5}, {63, 63, 63}};

    static const struct
    {
        int width;
        int height;
        int depth;
    } sizes3D[] = {{4, 8, 8}, {32, 64, 16}, {128, 32, 64}, {3, 7, 5}, {63, 63, 63}};

    static const struct
    {
        const char *name;
        uint32_t format;
    } filterableFormatsByType[] = {{"rgba16f", GL_RGBA16F},
                                   {"r11f_g11f_b10f", GL_R11F_G11F_B10F},
                                   {"rgb9_e5", GL_RGB9_E5},
                                   {"rgba8", GL_RGBA8},
                                   {"rgba8_snorm", GL_RGBA8_SNORM},
                                   {"rgb565", GL_RGB565},
                                   {"rgba4", GL_RGBA4},
                                   {"rgb5_a1", GL_RGB5_A1},
                                   {"srgb8_alpha8", GL_SRGB8_ALPHA8},
                                   {"srgb_r8", GL_SR8_EXT},
                                   {"srgb_rg8", GL_SRG8_EXT},
                                   {"rgb10_a2", GL_RGB10_A2},
                                   {"bgra", GL_BGRA},
                                   {"bgra8", GL_BGRA8_EXT}};

    // 2D texture filtering.
    {
        tcu::TestCaseGroup *group2D = new tcu::TestCaseGroup(m_testCtx, "2d", "2D Texture Filtering");
        addChild(group2D);

        // Formats.
        tcu::TestCaseGroup *formatsGroup = new tcu::TestCaseGroup(m_testCtx, "formats", "2D Texture Formats");
        group2D->addChild(formatsGroup);
        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(filterableFormatsByType); fmtNdx++)
        {
            for (int filterNdx = 0; filterNdx < DE_LENGTH_OF_ARRAY(minFilterModes); filterNdx++)
            {
                uint32_t minFilter     = minFilterModes[filterNdx].mode;
                const char *filterName = minFilterModes[filterNdx].name;
                uint32_t format        = filterableFormatsByType[fmtNdx].format;
                const char *formatName = filterableFormatsByType[fmtNdx].name;
                bool isMipmap          = minFilter != GL_NEAREST && minFilter != GL_LINEAR;
                uint32_t magFilter     = isMipmap ? GL_LINEAR : minFilter;
                string name            = string(formatName) + "_" + filterName;
                uint32_t wrapS         = GL_REPEAT;
                uint32_t wrapT         = GL_REPEAT;
                int width              = 64;
                int height             = 64;

                formatsGroup->addChild(new Texture2DFilteringCase(
                    m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.c_str(), "", minFilter,
                    magFilter, wrapS, wrapT, format, width, height));
            }
        }

        // ETC1 format.
        {
            std::vector<std::string> filenames;
            for (int i = 0; i <= 7; i++)
                filenames.push_back(string("data/etc1/photo_helsinki_mip_") + de::toString(i) + ".pkm");

            for (int filterNdx = 0; filterNdx < DE_LENGTH_OF_ARRAY(minFilterModes); filterNdx++)
            {
                uint32_t minFilter     = minFilterModes[filterNdx].mode;
                const char *filterName = minFilterModes[filterNdx].name;
                bool isMipmap          = minFilter != GL_NEAREST && minFilter != GL_LINEAR;
                uint32_t magFilter     = isMipmap ? GL_LINEAR : minFilter;
                string name            = string("etc1_rgb8_") + filterName;
                uint32_t wrapS         = GL_REPEAT;
                uint32_t wrapT         = GL_REPEAT;

                formatsGroup->addChild(new Texture2DFilteringCase(m_testCtx, m_context.getRenderContext(),
                                                                  m_context.getContextInfo(), name.c_str(), "",
                                                                  minFilter, magFilter, wrapS, wrapT, filenames));
            }
        }

        // Sizes.
        tcu::TestCaseGroup *sizesGroup = new tcu::TestCaseGroup(m_testCtx, "sizes", "Texture Sizes");
        group2D->addChild(sizesGroup);
        for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes2D); sizeNdx++)
        {
            for (int filterNdx = 0; filterNdx < DE_LENGTH_OF_ARRAY(minFilterModes); filterNdx++)
            {
                uint32_t minFilter     = minFilterModes[filterNdx].mode;
                const char *filterName = minFilterModes[filterNdx].name;
                uint32_t format        = GL_RGBA8;
                bool isMipmap          = minFilter != GL_NEAREST && minFilter != GL_LINEAR;
                uint32_t magFilter     = isMipmap ? GL_LINEAR : minFilter;
                uint32_t wrapS         = GL_REPEAT;
                uint32_t wrapT         = GL_REPEAT;
                int width              = sizes2D[sizeNdx].width;
                int height             = sizes2D[sizeNdx].height;
                string name            = de::toString(width) + "x" + de::toString(height) + "_" + filterName;

                sizesGroup->addChild(new Texture2DFilteringCase(m_testCtx, m_context.getRenderContext(),
                                                                m_context.getContextInfo(), name.c_str(), "", minFilter,
                                                                magFilter, wrapS, wrapT, format, width, height));
            }
        }

        // Wrap modes.
        tcu::TestCaseGroup *combinationsGroup =
            new tcu::TestCaseGroup(m_testCtx, "combinations", "Filter and wrap mode combinations");
        group2D->addChild(combinationsGroup);
        for (int minFilterNdx = 0; minFilterNdx < DE_LENGTH_OF_ARRAY(minFilterModes); minFilterNdx++)
        {
            for (int magFilterNdx = 0; magFilterNdx < DE_LENGTH_OF_ARRAY(magFilterModes); magFilterNdx++)
            {
                for (int wrapSNdx = 0; wrapSNdx < DE_LENGTH_OF_ARRAY(wrapModes); wrapSNdx++)
                {
                    for (int wrapTNdx = 0; wrapTNdx < DE_LENGTH_OF_ARRAY(wrapModes); wrapTNdx++)
                    {
                        uint32_t minFilter = minFilterModes[minFilterNdx].mode;
                        uint32_t magFilter = magFilterModes[magFilterNdx].mode;
                        uint32_t format    = GL_RGBA8;
                        uint32_t wrapS     = wrapModes[wrapSNdx].mode;
                        uint32_t wrapT     = wrapModes[wrapTNdx].mode;
                        int width          = 63;
                        int height         = 57;
                        string name        = string(minFilterModes[minFilterNdx].name) + "_" +
                                      magFilterModes[magFilterNdx].name + "_" + wrapModes[wrapSNdx].name + "_" +
                                      wrapModes[wrapTNdx].name;

                        combinationsGroup->addChild(new Texture2DFilteringCase(
                            m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.c_str(), "",
                            minFilter, magFilter, wrapS, wrapT, format, width, height));
                    }
                }
            }
        }
    }

    // Cube map texture filtering.
    {
        tcu::TestCaseGroup *groupCube = new tcu::TestCaseGroup(m_testCtx, "cube", "Cube Map Texture Filtering");
        addChild(groupCube);

        // Formats.
        tcu::TestCaseGroup *formatsGroup = new tcu::TestCaseGroup(m_testCtx, "formats", "2D Texture Formats");
        groupCube->addChild(formatsGroup);
        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(filterableFormatsByType); fmtNdx++)
        {
            for (int filterNdx = 0; filterNdx < DE_LENGTH_OF_ARRAY(minFilterModes); filterNdx++)
            {
                uint32_t minFilter     = minFilterModes[filterNdx].mode;
                const char *filterName = minFilterModes[filterNdx].name;
                uint32_t format        = filterableFormatsByType[fmtNdx].format;
                const char *formatName = filterableFormatsByType[fmtNdx].name;
                bool isMipmap          = minFilter != GL_NEAREST && minFilter != GL_LINEAR;
                uint32_t magFilter     = isMipmap ? GL_LINEAR : minFilter;
                string name            = string(formatName) + "_" + filterName;
                uint32_t wrapS         = GL_REPEAT;
                uint32_t wrapT         = GL_REPEAT;
                int width              = 64;
                int height             = 64;

                formatsGroup->addChild(new TextureCubeFilteringCase(
                    m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.c_str(), "", minFilter,
                    magFilter, wrapS, wrapT, false /* always sample exterior as well */, format, width, height));
            }
        }

        // ETC1 format.
        {
            static const char *faceExt[] = {"neg_x", "pos_x", "neg_y", "pos_y", "neg_z", "pos_z"};

            const int numLevels = 7;
            vector<string> filenames;
            for (int level = 0; level < numLevels; level++)
                for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
                    filenames.push_back(string("data/etc1/skybox_") + faceExt[face] + "_mip_" + de::toString(level) +
                                        ".pkm");

            for (int filterNdx = 0; filterNdx < DE_LENGTH_OF_ARRAY(minFilterModes); filterNdx++)
            {
                uint32_t minFilter     = minFilterModes[filterNdx].mode;
                const char *filterName = minFilterModes[filterNdx].name;
                bool isMipmap          = minFilter != GL_NEAREST && minFilter != GL_LINEAR;
                uint32_t magFilter     = isMipmap ? GL_LINEAR : minFilter;
                string name            = string("etc1_rgb8_") + filterName;
                uint32_t wrapS         = GL_REPEAT;
                uint32_t wrapT         = GL_REPEAT;

                formatsGroup->addChild(new TextureCubeFilteringCase(
                    m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.c_str(), "", minFilter,
                    magFilter, wrapS, wrapT, false /* always sample exterior as well */, filenames));
            }
        }

        // Sizes.
        tcu::TestCaseGroup *sizesGroup = new tcu::TestCaseGroup(m_testCtx, "sizes", "Texture Sizes");
        groupCube->addChild(sizesGroup);
        for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizesCube); sizeNdx++)
        {
            for (int filterNdx = 0; filterNdx < DE_LENGTH_OF_ARRAY(minFilterModes); filterNdx++)
            {
                uint32_t minFilter     = minFilterModes[filterNdx].mode;
                const char *filterName = minFilterModes[filterNdx].name;
                uint32_t format        = GL_RGBA8;
                bool isMipmap          = minFilter != GL_NEAREST && minFilter != GL_LINEAR;
                uint32_t magFilter     = isMipmap ? GL_LINEAR : minFilter;
                uint32_t wrapS         = GL_REPEAT;
                uint32_t wrapT         = GL_REPEAT;
                int width              = sizesCube[sizeNdx].width;
                int height             = sizesCube[sizeNdx].height;
                string name            = de::toString(width) + "x" + de::toString(height) + "_" + filterName;

                sizesGroup->addChild(new TextureCubeFilteringCase(
                    m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.c_str(), "", minFilter,
                    magFilter, wrapS, wrapT, false, format, width, height));
            }
        }

        // Filter/wrap mode combinations.
        tcu::TestCaseGroup *combinationsGroup =
            new tcu::TestCaseGroup(m_testCtx, "combinations", "Filter and wrap mode combinations");
        groupCube->addChild(combinationsGroup);
        for (int minFilterNdx = 0; minFilterNdx < DE_LENGTH_OF_ARRAY(minFilterModes); minFilterNdx++)
        {
            for (int magFilterNdx = 0; magFilterNdx < DE_LENGTH_OF_ARRAY(magFilterModes); magFilterNdx++)
            {
                for (int wrapSNdx = 0; wrapSNdx < DE_LENGTH_OF_ARRAY(wrapModes); wrapSNdx++)
                {
                    for (int wrapTNdx = 0; wrapTNdx < DE_LENGTH_OF_ARRAY(wrapModes); wrapTNdx++)
                    {
                        uint32_t minFilter = minFilterModes[minFilterNdx].mode;
                        uint32_t magFilter = magFilterModes[magFilterNdx].mode;
                        uint32_t format    = GL_RGBA8;
                        uint32_t wrapS     = wrapModes[wrapSNdx].mode;
                        uint32_t wrapT     = wrapModes[wrapTNdx].mode;
                        int width          = 63;
                        int height         = 63;
                        string name        = string(minFilterModes[minFilterNdx].name) + "_" +
                                      magFilterModes[magFilterNdx].name + "_" + wrapModes[wrapSNdx].name + "_" +
                                      wrapModes[wrapTNdx].name;

                        combinationsGroup->addChild(new TextureCubeFilteringCase(
                            m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.c_str(), "",
                            minFilter, magFilter, wrapS, wrapT, false, format, width, height));
                    }
                }
            }
        }

        // Cases with no visible cube edges.
        tcu::TestCaseGroup *onlyFaceInteriorGroup =
            new tcu::TestCaseGroup(m_testCtx, "no_edges_visible", "Don't sample anywhere near a face's edges");
        groupCube->addChild(onlyFaceInteriorGroup);

        for (int isLinearI = 0; isLinearI <= 1; isLinearI++)
        {
            bool isLinear   = isLinearI != 0;
            uint32_t filter = isLinear ? GL_LINEAR : GL_NEAREST;

            onlyFaceInteriorGroup->addChild(new TextureCubeFilteringCase(
                m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), isLinear ? "linear" : "nearest",
                "", filter, filter, GL_REPEAT, GL_REPEAT, true, GL_RGBA8, 63, 63));
        }
    }

    // 2D array texture filtering.
    {
        tcu::TestCaseGroup *const group2DArray =
            new tcu::TestCaseGroup(m_testCtx, "2d_array", "2D Array Texture Filtering");
        addChild(group2DArray);

        // Formats.
        tcu::TestCaseGroup *const formatsGroup =
            new tcu::TestCaseGroup(m_testCtx, "formats", "2D Array Texture Formats");
        group2DArray->addChild(formatsGroup);
        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(filterableFormatsByType); fmtNdx++)
        {
            for (int filterNdx = 0; filterNdx < DE_LENGTH_OF_ARRAY(minFilterModes); filterNdx++)
            {
                uint32_t minFilter     = minFilterModes[filterNdx].mode;
                const char *filterName = minFilterModes[filterNdx].name;
                uint32_t format        = filterableFormatsByType[fmtNdx].format;
                const char *formatName = filterableFormatsByType[fmtNdx].name;
                bool isMipmap          = minFilter != GL_NEAREST && minFilter != GL_LINEAR;
                uint32_t magFilter     = isMipmap ? GL_LINEAR : minFilter;
                string name            = string(formatName) + "_" + filterName;
                uint32_t wrapS         = GL_REPEAT;
                uint32_t wrapT         = GL_REPEAT;
                int width              = 128;
                int height             = 128;
                int numLayers          = 8;

                formatsGroup->addChild(new Texture2DArrayFilteringCase(
                    m_context, name.c_str(), "", minFilter, magFilter, wrapS, wrapT, format, width, height, numLayers));
            }
        }

        // Sizes.
        tcu::TestCaseGroup *sizesGroup = new tcu::TestCaseGroup(m_testCtx, "sizes", "Texture Sizes");
        group2DArray->addChild(sizesGroup);
        for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes2DArray); sizeNdx++)
        {
            for (int filterNdx = 0; filterNdx < DE_LENGTH_OF_ARRAY(minFilterModes); filterNdx++)
            {
                uint32_t minFilter     = minFilterModes[filterNdx].mode;
                const char *filterName = minFilterModes[filterNdx].name;
                uint32_t format        = GL_RGBA8;
                bool isMipmap          = minFilter != GL_NEAREST && minFilter != GL_LINEAR;
                uint32_t magFilter     = isMipmap ? GL_LINEAR : minFilter;
                uint32_t wrapS         = GL_REPEAT;
                uint32_t wrapT         = GL_REPEAT;
                int width              = sizes2DArray[sizeNdx].width;
                int height             = sizes2DArray[sizeNdx].height;
                int numLayers          = sizes2DArray[sizeNdx].numLayers;
                string name =
                    de::toString(width) + "x" + de::toString(height) + "x" + de::toString(numLayers) + "_" + filterName;

                sizesGroup->addChild(new Texture2DArrayFilteringCase(m_context, name.c_str(), "", minFilter, magFilter,
                                                                     wrapS, wrapT, format, width, height, numLayers));
            }
        }

        // Wrap modes.
        tcu::TestCaseGroup *const combinationsGroup =
            new tcu::TestCaseGroup(m_testCtx, "combinations", "Filter and wrap mode combinations");
        group2DArray->addChild(combinationsGroup);
        for (int minFilterNdx = 0; minFilterNdx < DE_LENGTH_OF_ARRAY(minFilterModes); minFilterNdx++)
        {
            for (int magFilterNdx = 0; magFilterNdx < DE_LENGTH_OF_ARRAY(magFilterModes); magFilterNdx++)
            {
                for (int wrapSNdx = 0; wrapSNdx < DE_LENGTH_OF_ARRAY(wrapModes); wrapSNdx++)
                {
                    for (int wrapTNdx = 0; wrapTNdx < DE_LENGTH_OF_ARRAY(wrapModes); wrapTNdx++)
                    {
                        uint32_t minFilter = minFilterModes[minFilterNdx].mode;
                        uint32_t magFilter = magFilterModes[magFilterNdx].mode;
                        uint32_t format    = GL_RGBA8;
                        uint32_t wrapS     = wrapModes[wrapSNdx].mode;
                        uint32_t wrapT     = wrapModes[wrapTNdx].mode;
                        int width          = 123;
                        int height         = 107;
                        int numLayers      = 7;
                        string name        = string(minFilterModes[minFilterNdx].name) + "_" +
                                      magFilterModes[magFilterNdx].name + "_" + wrapModes[wrapSNdx].name + "_" +
                                      wrapModes[wrapTNdx].name;

                        combinationsGroup->addChild(new Texture2DArrayFilteringCase(m_context, name.c_str(), "",
                                                                                    minFilter, magFilter, wrapS, wrapT,
                                                                                    format, width, height, numLayers));
                    }
                }
            }
        }
    }

    // 3D texture filtering.
    {
        tcu::TestCaseGroup *group3D = new tcu::TestCaseGroup(m_testCtx, "3d", "3D Texture Filtering");
        addChild(group3D);

        // Formats.
        tcu::TestCaseGroup *formatsGroup = new tcu::TestCaseGroup(m_testCtx, "formats", "3D Texture Formats");
        group3D->addChild(formatsGroup);
        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(filterableFormatsByType); fmtNdx++)
        {
            for (int filterNdx = 0; filterNdx < DE_LENGTH_OF_ARRAY(minFilterModes); filterNdx++)
            {
                uint32_t minFilter     = minFilterModes[filterNdx].mode;
                const char *filterName = minFilterModes[filterNdx].name;
                uint32_t format        = filterableFormatsByType[fmtNdx].format;
                const char *formatName = filterableFormatsByType[fmtNdx].name;
                bool isMipmap          = minFilter != GL_NEAREST && minFilter != GL_LINEAR;
                uint32_t magFilter     = isMipmap ? GL_LINEAR : minFilter;
                string name            = string(formatName) + "_" + filterName;
                uint32_t wrapS         = GL_REPEAT;
                uint32_t wrapT         = GL_REPEAT;
                uint32_t wrapR         = GL_REPEAT;
                int width              = 64;
                int height             = 64;
                int depth              = 64;

                formatsGroup->addChild(new Texture3DFilteringCase(m_context, name.c_str(), "", minFilter, magFilter,
                                                                  wrapS, wrapT, wrapR, format, width, height, depth));
            }
        }

        // Sizes.
        tcu::TestCaseGroup *sizesGroup = new tcu::TestCaseGroup(m_testCtx, "sizes", "Texture Sizes");
        group3D->addChild(sizesGroup);
        for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes3D); sizeNdx++)
        {
            for (int filterNdx = 0; filterNdx < DE_LENGTH_OF_ARRAY(minFilterModes); filterNdx++)
            {
                uint32_t minFilter     = minFilterModes[filterNdx].mode;
                const char *filterName = minFilterModes[filterNdx].name;
                uint32_t format        = GL_RGBA8;
                bool isMipmap          = minFilter != GL_NEAREST && minFilter != GL_LINEAR;
                uint32_t magFilter     = isMipmap ? GL_LINEAR : minFilter;
                uint32_t wrapS         = GL_REPEAT;
                uint32_t wrapT         = GL_REPEAT;
                uint32_t wrapR         = GL_REPEAT;
                int width              = sizes3D[sizeNdx].width;
                int height             = sizes3D[sizeNdx].height;
                int depth              = sizes3D[sizeNdx].depth;
                string name =
                    de::toString(width) + "x" + de::toString(height) + "x" + de::toString(depth) + "_" + filterName;

                sizesGroup->addChild(new Texture3DFilteringCase(m_context, name.c_str(), "", minFilter, magFilter,
                                                                wrapS, wrapT, wrapR, format, width, height, depth));
            }
        }

        // Wrap modes.
        tcu::TestCaseGroup *combinationsGroup =
            new tcu::TestCaseGroup(m_testCtx, "combinations", "Filter and wrap mode combinations");
        group3D->addChild(combinationsGroup);
        for (int minFilterNdx = 0; minFilterNdx < DE_LENGTH_OF_ARRAY(minFilterModes); minFilterNdx++)
        {
            for (int magFilterNdx = 0; magFilterNdx < DE_LENGTH_OF_ARRAY(magFilterModes); magFilterNdx++)
            {
                for (int wrapSNdx = 0; wrapSNdx < DE_LENGTH_OF_ARRAY(wrapModes); wrapSNdx++)
                {
                    for (int wrapTNdx = 0; wrapTNdx < DE_LENGTH_OF_ARRAY(wrapModes); wrapTNdx++)
                    {
                        for (int wrapRNdx = 0; wrapRNdx < DE_LENGTH_OF_ARRAY(wrapModes); wrapRNdx++)
                        {
                            uint32_t minFilter = minFilterModes[minFilterNdx].mode;
                            uint32_t magFilter = magFilterModes[magFilterNdx].mode;
                            uint32_t format    = GL_RGBA8;
                            uint32_t wrapS     = wrapModes[wrapSNdx].mode;
                            uint32_t wrapT     = wrapModes[wrapTNdx].mode;
                            uint32_t wrapR     = wrapModes[wrapRNdx].mode;
                            int width          = 63;
                            int height         = 57;
                            int depth          = 67;
                            string name        = string(minFilterModes[minFilterNdx].name) + "_" +
                                          magFilterModes[magFilterNdx].name + "_" + wrapModes[wrapSNdx].name + "_" +
                                          wrapModes[wrapTNdx].name + "_" + wrapModes[wrapRNdx].name;

                            combinationsGroup->addChild(
                                new Texture3DFilteringCase(m_context, name.c_str(), "", minFilter, magFilter, wrapS,
                                                           wrapT, wrapR, format, width, height, depth));
                        }
                    }
                }
            }
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
