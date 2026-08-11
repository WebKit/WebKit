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
 * \brief Shadow texture lookup tests.
 *//*--------------------------------------------------------------------*/

#include "es3fTextureShadowTests.hpp"
#include "gluTexture.hpp"
#include "gluPixelTransfer.hpp"
#include "gluTextureUtil.hpp"
#include "glsTextureTestUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTexCompareVerifier.hpp"
#include "tcuVectorUtil.hpp"
#include "deString.h"
#include "deMath.h"
#include "deStringUtil.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::string;
using std::vector;
using tcu::TestLog;
using namespace deqp::gls::TextureTestUtil;
using namespace glu::TextureTestUtil;

enum
{
    TEX2D_VIEWPORT_WIDTH      = 64,
    TEX2D_VIEWPORT_HEIGHT     = 64,
    TEX2D_MIN_VIEWPORT_WIDTH  = 64,
    TEX2D_MIN_VIEWPORT_HEIGHT = 64
};

static bool isFloatingPointDepthFormat(const tcu::TextureFormat &format)
{
    // Only two depth and depth-stencil formats are floating point
    return (format.order == tcu::TextureFormat::D && format.type == tcu::TextureFormat::FLOAT) ||
           (format.order == tcu::TextureFormat::DS && format.type == tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV);
}

static void clampFloatingPointTexture(const tcu::PixelBufferAccess &access)
{
    DE_ASSERT(isFloatingPointDepthFormat(access.getFormat()));

    for (int z = 0; z < access.getDepth(); ++z)
        for (int y = 0; y < access.getHeight(); ++y)
            for (int x = 0; x < access.getWidth(); ++x)
                access.setPixDepth(de::clamp(access.getPixDepth(x, y, z), 0.0f, 1.0f), x, y, z);
}

static void clampFloatingPointTexture(tcu::Texture2D &target)
{
    for (int level = 0; level < target.getNumLevels(); ++level)
        if (!target.isLevelEmpty(level))
            clampFloatingPointTexture(target.getLevel(level));
}

static void clampFloatingPointTexture(tcu::Texture2DArray &target)
{
    for (int level = 0; level < target.getNumLevels(); ++level)
        if (!target.isLevelEmpty(level))
            clampFloatingPointTexture(target.getLevel(level));
}

static void clampFloatingPointTexture(tcu::TextureCube &target)
{
    for (int level = 0; level < target.getNumLevels(); ++level)
        for (int face = tcu::CUBEFACE_NEGATIVE_X; face < tcu::CUBEFACE_LAST; ++face)
            clampFloatingPointTexture(target.getLevelFace(level, (tcu::CubeFace)face));
}

template <typename TextureType>
bool verifyTexCompareResult(tcu::TestContext &testCtx, const tcu::ConstPixelBufferAccess &result,
                            const TextureType &src, const float *texCoord, const ReferenceParams &sampleParams,
                            const tcu::TexComparePrecision &comparePrec, const tcu::LodPrecision &lodPrec,
                            const tcu::PixelFormat &pixelFormat)
{
    tcu::TestLog &log = testCtx.getLog();
    tcu::Surface reference(result.getWidth(), result.getHeight());
    tcu::Surface errorMask(result.getWidth(), result.getHeight());
    const tcu::IVec4 nonShadowBits     = tcu::max(getBitsVec(pixelFormat) - 1, tcu::IVec4(0));
    const tcu::Vec3 nonShadowThreshold = tcu::computeFixedPointThreshold(nonShadowBits).swizzle(1, 2, 3);
    int numFailedPixels;

    // sampleTexture() expects source image to be the same state as it would be in a GL implementation, that is
    // the floating point depth values should be in [0, 1] range as data is clamped during texture upload. Since
    // we don't have a separate "uploading" phase and just reuse the buffer we used for GL-upload, do the clamping
    // here if necessary.

    if (isFloatingPointDepthFormat(src.getFormat()))
    {
        TextureType clampedSource(src);

        clampFloatingPointTexture(clampedSource);

        // sample clamped values

        sampleTexture(tcu::SurfaceAccess(reference, pixelFormat), clampedSource, texCoord, sampleParams);
        numFailedPixels = computeTextureCompareDiff(result, reference.getAccess(), errorMask.getAccess(), clampedSource,
                                                    texCoord, sampleParams, comparePrec, lodPrec, nonShadowThreshold);
    }
    else
    {
        // sample raw values (they are guaranteed to be in [0, 1] range as the format cannot represent any other values)

        sampleTexture(tcu::SurfaceAccess(reference, pixelFormat), src, texCoord, sampleParams);
        numFailedPixels = computeTextureCompareDiff(result, reference.getAccess(), errorMask.getAccess(), src, texCoord,
                                                    sampleParams, comparePrec, lodPrec, nonShadowThreshold);
    }

    if (numFailedPixels > 0)
        log << TestLog::Message << "ERROR: Result verification failed, got " << numFailedPixels << " invalid pixels!"
            << TestLog::EndMessage;

    log << TestLog::ImageSet("VerifyResult", "Verification result")
        << TestLog::Image("Rendered", "Rendered image", result);

    if (numFailedPixels > 0)
    {
        log << TestLog::Image("Reference", "Ideal reference image", reference)
            << TestLog::Image("ErrorMask", "Error mask", errorMask);
    }

    log << TestLog::EndImageSet;

    return numFailedPixels == 0;
}

class Texture2DShadowCase : public TestCase
{
public:
    Texture2DShadowCase(Context &context, const char *name, const char *desc, uint32_t minFilter, uint32_t magFilter,
                        uint32_t wrapS, uint32_t wrapT, uint32_t format, int width, int height, uint32_t compareFunc);
    ~Texture2DShadowCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture2DShadowCase(const Texture2DShadowCase &other);
    Texture2DShadowCase &operator=(const Texture2DShadowCase &other);

    const uint32_t m_minFilter;
    const uint32_t m_magFilter;
    const uint32_t m_wrapS;
    const uint32_t m_wrapT;
    const uint32_t m_format;
    const int m_width;
    const int m_height;
    const uint32_t m_compareFunc;

    struct FilterCase
    {
        const glu::Texture2D *texture;
        tcu::Vec2 minCoord;
        tcu::Vec2 maxCoord;
        float ref;

        FilterCase(void) : texture(nullptr), ref(0.0f)
        {
        }

        FilterCase(const glu::Texture2D *tex_, const float ref_, const tcu::Vec2 &minCoord_, const tcu::Vec2 &maxCoord_)
            : texture(tex_)
            , minCoord(minCoord_)
            , maxCoord(maxCoord_)
            , ref(ref_)
        {
        }
    };

    std::vector<glu::Texture2D *> m_textures;
    std::vector<FilterCase> m_cases;

    TextureRenderer m_renderer;

    int m_caseNdx;
};

Texture2DShadowCase::Texture2DShadowCase(Context &context, const char *name, const char *desc, uint32_t minFilter,
                                         uint32_t magFilter, uint32_t wrapS, uint32_t wrapT, uint32_t format, int width,
                                         int height, uint32_t compareFunc)
    : TestCase(context, name, desc)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_format(format)
    , m_width(width)
    , m_height(height)
    , m_compareFunc(compareFunc)
    , m_renderer(context.getRenderContext(), context.getTestContext().getLog(), glu::GLSL_VERSION_300_ES,
                 glu::PRECISION_HIGHP)
    , m_caseNdx(0)
{
}

Texture2DShadowCase::~Texture2DShadowCase(void)
{
    deinit();
}

void Texture2DShadowCase::init(void)
{
    try
    {
        // Create 2 textures.
        m_textures.reserve(2);
        m_textures.push_back(new glu::Texture2D(m_context.getRenderContext(), m_format, m_width, m_height));
        m_textures.push_back(new glu::Texture2D(m_context.getRenderContext(), m_format, m_width, m_height));

        int numLevels = m_textures[0]->getRefTexture().getNumLevels();

        // Fill first gradient texture.
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            m_textures[0]->getRefTexture().allocLevel(levelNdx);
            tcu::fillWithComponentGradients(m_textures[0]->getRefTexture().getLevel(levelNdx),
                                            tcu::Vec4(-0.5f, -0.5f, -0.5f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f));
        }

        // Fill second with grid texture.
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            uint32_t step   = 0x00ffffff / numLevels;
            uint32_t rgb    = step * levelNdx;
            uint32_t colorA = 0xff000000 | rgb;
            uint32_t colorB = 0xff000000 | ~rgb;

            m_textures[1]->getRefTexture().allocLevel(levelNdx);
            tcu::fillWithGrid(m_textures[1]->getRefTexture().getLevel(levelNdx), 4, tcu::RGBA(colorA).toVec(),
                              tcu::RGBA(colorB).toVec());
        }

        // Upload.
        for (std::vector<glu::Texture2D *>::iterator i = m_textures.begin(); i != m_textures.end(); i++)
            (*i)->upload();
    }
    catch (const std::exception &)
    {
        // Clean up to save memory.
        Texture2DShadowCase::deinit();
        throw;
    }

    // Compute cases.
    {
        const float refInRangeUpper     = (m_compareFunc == GL_EQUAL || m_compareFunc == GL_NOTEQUAL) ? 1.0f : 0.5f;
        const float refInRangeLower     = (m_compareFunc == GL_EQUAL || m_compareFunc == GL_NOTEQUAL) ? 0.0f : 0.5f;
        const float refOutOfBoundsUpper = 1.1f; // !< lookup function should clamp values to [0, 1] range
        const float refOutOfBoundsLower = -0.1f;

        const struct
        {
            int texNdx;
            float ref;
            float lodX;
            float lodY;
            float oX;
            float oY;
        } cases[] = {
            {0, refInRangeUpper, 1.6f, 2.9f, -1.0f, -2.7f},
            {0, refInRangeLower, -2.0f, -1.35f, -0.2f, 0.7f},
            {1, refInRangeUpper, 0.14f, 0.275f, -1.5f, -1.1f},
            {1, refInRangeLower, -0.92f, -2.64f, 0.4f, -0.1f},
            {1, refOutOfBoundsUpper, -0.39f, -0.52f, 0.65f, 0.87f},
            {1, refOutOfBoundsLower, -1.55f, 0.65f, 0.35f, 0.91f},
        };

        const float viewportW = (float)de::min<int>(TEX2D_VIEWPORT_WIDTH, m_context.getRenderTarget().getWidth());
        const float viewportH = (float)de::min<int>(TEX2D_VIEWPORT_HEIGHT, m_context.getRenderTarget().getHeight());

        for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); caseNdx++)
        {
            const int texNdx = de::clamp(cases[caseNdx].texNdx, 0, (int)m_textures.size() - 1);
            const float ref  = cases[caseNdx].ref;
            const float lodX = cases[caseNdx].lodX;
            const float lodY = cases[caseNdx].lodY;
            const float oX   = cases[caseNdx].oX;
            const float oY   = cases[caseNdx].oY;
            const float sX   = deFloatExp2(lodX) * viewportW / float(m_textures[texNdx]->getRefTexture().getWidth());
            const float sY   = deFloatExp2(lodY) * viewportH / float(m_textures[texNdx]->getRefTexture().getHeight());

            m_cases.push_back(FilterCase(m_textures[texNdx], ref, tcu::Vec2(oX, oY), tcu::Vec2(oX + sX, oY + sY)));
        }
    }

    m_caseNdx = 0;
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
}

void Texture2DShadowCase::deinit(void)
{
    for (std::vector<glu::Texture2D *>::iterator i = m_textures.begin(); i != m_textures.end(); i++)
        delete *i;
    m_textures.clear();

    m_renderer.clear();
    m_cases.clear();
}

Texture2DShadowCase::IterateResult Texture2DShadowCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const RandomViewport viewport(m_context.getRenderTarget(), TEX2D_VIEWPORT_WIDTH, TEX2D_VIEWPORT_HEIGHT,
                                  deStringHash(getName()) ^ deInt32Hash(m_caseNdx));
    const FilterCase &curCase = m_cases[m_caseNdx];
    const tcu::ScopedLogSection section(m_testCtx.getLog(), string("Test") + de::toString(m_caseNdx),
                                        string("Test ") + de::toString(m_caseNdx));
    ReferenceParams sampleParams(TEXTURETYPE_2D);
    tcu::Surface rendered(viewport.width, viewport.height);
    vector<float> texCoord;

    if (viewport.width < TEX2D_MIN_VIEWPORT_WIDTH || viewport.height < TEX2D_MIN_VIEWPORT_HEIGHT)
        throw tcu::NotSupportedError("Too small render target", "", __FILE__, __LINE__);

    // Setup params for reference.
    sampleParams.sampler         = glu::mapGLSampler(m_wrapS, m_wrapT, m_minFilter, m_magFilter);
    sampleParams.sampler.compare = glu::mapGLCompareFunc(m_compareFunc);
    sampleParams.samplerType     = SAMPLERTYPE_SHADOW;
    sampleParams.lodMode         = LODMODE_EXACT;
    sampleParams.ref             = curCase.ref;

    m_testCtx.getLog() << TestLog::Message << "Compare reference value =  " << sampleParams.ref << TestLog::EndMessage;

    // Compute texture coordinates.
    m_testCtx.getLog() << TestLog::Message << "Texture coordinates: " << curCase.minCoord << " -> " << curCase.maxCoord
                       << TestLog::EndMessage;
    computeQuadTexCoord2D(texCoord, curCase.minCoord, curCase.maxCoord);

    gl.bindTexture(GL_TEXTURE_2D, curCase.texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_magFilter);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapS);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapT);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, m_compareFunc);

    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);
    m_renderer.renderQuad(0, &texCoord[0], sampleParams);
    glu::readPixels(m_context.getRenderContext(), viewport.x, viewport.y, rendered.getAccess());

    {
        const tcu::PixelFormat pixelFormat = m_context.getRenderTarget().getPixelFormat();
        tcu::LodPrecision lodPrecision;
        tcu::TexComparePrecision texComparePrecision;

        lodPrecision.derivateBits         = 18;
        lodPrecision.lodBits              = 6;
        texComparePrecision.coordBits     = tcu::IVec3(20, 20, 0);
        texComparePrecision.uvwBits       = tcu::IVec3(7, 7, 0);
        texComparePrecision.pcfBits       = 5;
        texComparePrecision.referenceBits = 16;
        texComparePrecision.resultBits    = pixelFormat.redBits - 1;

        const bool isHighQuality =
            verifyTexCompareResult(m_testCtx, rendered.getAccess(), curCase.texture->getRefTexture(), &texCoord[0],
                                   sampleParams, texComparePrecision, lodPrecision, pixelFormat);

        if (!isHighQuality)
        {
            m_testCtx.getLog() << TestLog::Message
                               << "Warning: Verification assuming high-quality PCF filtering failed."
                               << TestLog::EndMessage;

            lodPrecision.lodBits        = 4;
            texComparePrecision.uvwBits = tcu::IVec3(4, 4, 0);
            texComparePrecision.pcfBits = 0;

            const bool isOk =
                verifyTexCompareResult(m_testCtx, rendered.getAccess(), curCase.texture->getRefTexture(), &texCoord[0],
                                       sampleParams, texComparePrecision, lodPrecision, pixelFormat);

            if (!isOk)
            {
                m_testCtx.getLog()
                    << TestLog::Message
                    << "ERROR: Verification against low precision requirements failed, failing test case."
                    << TestLog::EndMessage;
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image verification failed");
            }
            else if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING, "Low-quality result");
        }
    }

    m_caseNdx += 1;
    return m_caseNdx < (int)m_cases.size() ? CONTINUE : STOP;
}

class TextureCubeShadowCase : public TestCase
{
public:
    TextureCubeShadowCase(Context &context, const char *name, const char *desc, uint32_t minFilter, uint32_t magFilter,
                          uint32_t wrapS, uint32_t wrapT, uint32_t format, int size, uint32_t compareFunc);
    ~TextureCubeShadowCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    TextureCubeShadowCase(const TextureCubeShadowCase &other);
    TextureCubeShadowCase &operator=(const TextureCubeShadowCase &other);

    const uint32_t m_minFilter;
    const uint32_t m_magFilter;
    const uint32_t m_wrapS;
    const uint32_t m_wrapT;

    const uint32_t m_format;
    const int m_size;

    const uint32_t m_compareFunc;

    struct FilterCase
    {
        const glu::TextureCube *texture;
        tcu::Vec2 bottomLeft;
        tcu::Vec2 topRight;
        float ref;

        FilterCase(void) : texture(nullptr), ref(0.0f)
        {
        }

        FilterCase(const glu::TextureCube *tex_, const float ref_, const tcu::Vec2 &bottomLeft_,
                   const tcu::Vec2 &topRight_)
            : texture(tex_)
            , bottomLeft(bottomLeft_)
            , topRight(topRight_)
            , ref(ref_)
        {
        }
    };

    glu::TextureCube *m_gradientTex;
    glu::TextureCube *m_gridTex;
    std::vector<FilterCase> m_cases;

    TextureRenderer m_renderer;

    int m_caseNdx;
};

TextureCubeShadowCase::TextureCubeShadowCase(Context &context, const char *name, const char *desc, uint32_t minFilter,
                                             uint32_t magFilter, uint32_t wrapS, uint32_t wrapT, uint32_t format,
                                             int size, uint32_t compareFunc)
    : TestCase(context, name, desc)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_format(format)
    , m_size(size)
    , m_compareFunc(compareFunc)
    , m_gradientTex(nullptr)
    , m_gridTex(nullptr)
    , m_renderer(context.getRenderContext(), context.getTestContext().getLog(), glu::GLSL_VERSION_300_ES,
                 glu::PRECISION_HIGHP)
    , m_caseNdx(0)
{
}

TextureCubeShadowCase::~TextureCubeShadowCase(void)
{
    TextureCubeShadowCase::deinit();
}

void TextureCubeShadowCase::init(void)
{
    try
    {
        DE_ASSERT(!m_gradientTex && !m_gridTex);

        int numLevels                  = deLog2Floor32(m_size) + 1;
        tcu::TextureFormat texFmt      = glu::mapGLInternalFormat(m_format);
        tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);
        tcu::Vec4 cBias                = fmtInfo.valueMin;
        tcu::Vec4 cScale               = fmtInfo.valueMax - fmtInfo.valueMin;

        // Create textures.
        m_gradientTex = new glu::TextureCube(m_context.getRenderContext(), m_format, m_size);
        m_gridTex     = new glu::TextureCube(m_context.getRenderContext(), m_format, m_size);

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
                m_gradientTex->getRefTexture().allocLevel((tcu::CubeFace)face, levelNdx);
                tcu::fillWithComponentGradients(
                    m_gradientTex->getRefTexture().getLevelFace(levelNdx, (tcu::CubeFace)face),
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

                m_gridTex->getRefTexture().allocLevel((tcu::CubeFace)face, levelNdx);
                tcu::fillWithGrid(m_gridTex->getRefTexture().getLevelFace(levelNdx, (tcu::CubeFace)face), 4,
                                  tcu::RGBA(colorA).toVec() * cScale + cBias,
                                  tcu::RGBA(colorB).toVec() * cScale + cBias);
            }
        }

        // Upload.
        m_gradientTex->upload();
        m_gridTex->upload();
    }
    catch (const std::exception &)
    {
        // Clean up to save memory.
        TextureCubeShadowCase::deinit();
        throw;
    }

    // Compute cases
    {
        const float refInRangeUpper     = (m_compareFunc == GL_EQUAL || m_compareFunc == GL_NOTEQUAL) ? 1.0f : 0.5f;
        const float refInRangeLower     = (m_compareFunc == GL_EQUAL || m_compareFunc == GL_NOTEQUAL) ? 0.0f : 0.5f;
        const float refOutOfBoundsUpper = 1.1f;
        const float refOutOfBoundsLower = -0.1f;
        const bool singleSample         = m_context.getRenderTarget().getNumSamples() == 0;

        if (singleSample)
            m_cases.push_back(FilterCase(m_gradientTex, refInRangeUpper, tcu::Vec2(-1.25f, -1.2f),
                                         tcu::Vec2(1.2f, 1.25f))); // minification
        else
            m_cases.push_back(FilterCase(
                m_gradientTex, refInRangeUpper, tcu::Vec2(-1.19f, -1.3f),
                tcu::Vec2(1.1f, 1.35f))); // minification - w/ tuned coordinates to avoid hitting triangle edges

        m_cases.push_back(FilterCase(m_gradientTex, refInRangeLower, tcu::Vec2(0.8f, 0.8f),
                                     tcu::Vec2(1.25f, 1.20f))); // magnification
        m_cases.push_back(
            FilterCase(m_gridTex, refInRangeUpper, tcu::Vec2(-1.19f, -1.3f), tcu::Vec2(1.1f, 1.35f))); // minification
        m_cases.push_back(
            FilterCase(m_gridTex, refInRangeLower, tcu::Vec2(-1.2f, -1.1f), tcu::Vec2(-0.8f, -0.8f))); // magnification
        m_cases.push_back(FilterCase(m_gridTex, refOutOfBoundsUpper, tcu::Vec2(-0.61f, -0.1f),
                                     tcu::Vec2(0.9f, 1.18f))); // reference value clamp, upper

        if (singleSample)
            m_cases.push_back(FilterCase(m_gridTex, refOutOfBoundsLower, tcu::Vec2(-0.75f, 1.0f),
                                         tcu::Vec2(0.05f, 0.75f))); // reference value clamp, lower
        else
            m_cases.push_back(FilterCase(m_gridTex, refOutOfBoundsLower, tcu::Vec2(-0.75f, 1.0f),
                                         tcu::Vec2(0.25f, 0.75f))); // reference value clamp, lower
    }

    m_caseNdx = 0;
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
}

void TextureCubeShadowCase::deinit(void)
{
    delete m_gradientTex;
    delete m_gridTex;

    m_gradientTex = nullptr;
    m_gridTex     = nullptr;

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

TextureCubeShadowCase::IterateResult TextureCubeShadowCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const int viewportSize   = 28;
    const RandomViewport viewport(m_context.getRenderTarget(), viewportSize, viewportSize,
                                  deStringHash(getName()) ^ deInt32Hash(m_caseNdx));
    const tcu::ScopedLogSection iterSection(m_testCtx.getLog(), string("Test") + de::toString(m_caseNdx),
                                            string("Test ") + de::toString(m_caseNdx));
    const FilterCase &curCase = m_cases[m_caseNdx];
    ReferenceParams sampleParams(TEXTURETYPE_CUBE);

    if (viewport.width < viewportSize || viewport.height < viewportSize)
        throw tcu::NotSupportedError("Too small render target", nullptr, __FILE__, __LINE__);

    // Setup texture
    gl.bindTexture(GL_TEXTURE_CUBE_MAP, curCase.texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, m_magFilter);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, m_wrapS);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, m_wrapT);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, m_compareFunc);

    // Other state
    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

    // Params for reference computation.
    sampleParams.sampler = glu::mapGLSampler(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, m_minFilter, m_magFilter);
    sampleParams.sampler.seamlessCubeMap = true;
    sampleParams.sampler.compare         = glu::mapGLCompareFunc(m_compareFunc);
    sampleParams.samplerType             = SAMPLERTYPE_SHADOW;
    sampleParams.lodMode                 = LODMODE_EXACT;
    sampleParams.ref                     = curCase.ref;

    m_testCtx.getLog() << TestLog::Message << "Compare reference value =  " << sampleParams.ref << "\n"
                       << "Coordinates: " << curCase.bottomLeft << " -> " << curCase.topRight << TestLog::EndMessage;

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

        glu::readPixels(m_context.getRenderContext(), viewport.x, viewport.y, result.getAccess());
        GLU_EXPECT_NO_ERROR(gl.getError(), "Read pixels");

        {
            const tcu::PixelFormat pixelFormat = m_context.getRenderTarget().getPixelFormat();
            tcu::LodPrecision lodPrecision;
            tcu::TexComparePrecision texComparePrecision;

            lodPrecision.derivateBits         = 10;
            lodPrecision.lodBits              = 5;
            texComparePrecision.coordBits     = tcu::IVec3(10, 10, 10);
            texComparePrecision.uvwBits       = tcu::IVec3(6, 6, 0);
            texComparePrecision.pcfBits       = 5;
            texComparePrecision.referenceBits = 16;
            texComparePrecision.resultBits    = pixelFormat.redBits - 1;

            const bool isHighQuality =
                verifyTexCompareResult(m_testCtx, result.getAccess(), curCase.texture->getRefTexture(), &texCoord[0],
                                       sampleParams, texComparePrecision, lodPrecision, pixelFormat);

            if (!isHighQuality)
            {
                m_testCtx.getLog() << TestLog::Message
                                   << "Warning: Verification assuming high-quality PCF filtering failed."
                                   << TestLog::EndMessage;

                lodPrecision.lodBits        = 4;
                texComparePrecision.uvwBits = tcu::IVec3(4, 4, 0);
                texComparePrecision.pcfBits = 0;

                const bool isOk =
                    verifyTexCompareResult(m_testCtx, result.getAccess(), curCase.texture->getRefTexture(),
                                           &texCoord[0], sampleParams, texComparePrecision, lodPrecision, pixelFormat);

                if (!isOk)
                {
                    m_testCtx.getLog()
                        << TestLog::Message
                        << "ERROR: Verification against low precision requirements failed, failing test case."
                        << TestLog::EndMessage;
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image verification failed");
                }
                else if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING, "Low-quality result");
            }
        }
    }

    m_caseNdx += 1;
    return m_caseNdx < (int)m_cases.size() ? CONTINUE : STOP;
}

class Texture2DArrayShadowCase : public TestCase
{
public:
    Texture2DArrayShadowCase(Context &context, const char *name, const char *desc, uint32_t minFilter,
                             uint32_t magFilter, uint32_t wrapS, uint32_t wrapT, uint32_t format, int width, int height,
                             int numLayers, uint32_t compareFunc);
    ~Texture2DArrayShadowCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture2DArrayShadowCase(const Texture2DArrayShadowCase &other);
    Texture2DArrayShadowCase &operator=(const Texture2DArrayShadowCase &other);

    const uint32_t m_minFilter;
    const uint32_t m_magFilter;
    const uint32_t m_wrapS;
    const uint32_t m_wrapT;

    const uint32_t m_format;
    const int m_width;
    const int m_height;
    const int m_numLayers;

    const uint32_t m_compareFunc;

    struct FilterCase
    {
        const glu::Texture2DArray *texture;
        tcu::Vec3 minCoord;
        tcu::Vec3 maxCoord;
        float ref;

        FilterCase(void) : texture(nullptr), ref(0.0f)
        {
        }

        FilterCase(const glu::Texture2DArray *tex_, float ref_, const tcu::Vec3 &minCoord_, const tcu::Vec3 &maxCoord_)
            : texture(tex_)
            , minCoord(minCoord_)
            , maxCoord(maxCoord_)
            , ref(ref_)
        {
        }
    };

    glu::Texture2DArray *m_gradientTex;
    glu::Texture2DArray *m_gridTex;
    std::vector<FilterCase> m_cases;

    TextureRenderer m_renderer;

    int m_caseNdx;
};

Texture2DArrayShadowCase::Texture2DArrayShadowCase(Context &context, const char *name, const char *desc,
                                                   uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                                                   uint32_t wrapT, uint32_t format, int width, int height,
                                                   int numLayers, uint32_t compareFunc)
    : TestCase(context, name, desc)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_format(format)
    , m_width(width)
    , m_height(height)
    , m_numLayers(numLayers)
    , m_compareFunc(compareFunc)
    , m_gradientTex(nullptr)
    , m_gridTex(nullptr)
    , m_renderer(context.getRenderContext(), context.getTestContext().getLog(), glu::GLSL_VERSION_300_ES,
                 glu::PRECISION_HIGHP)
    , m_caseNdx(0)
{
}

Texture2DArrayShadowCase::~Texture2DArrayShadowCase(void)
{
    Texture2DArrayShadowCase::deinit();
}

void Texture2DArrayShadowCase::init(void)
{
    try
    {
        tcu::TextureFormat texFmt      = glu::mapGLInternalFormat(m_format);
        tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);
        tcu::Vec4 cScale               = fmtInfo.valueMax - fmtInfo.valueMin;
        tcu::Vec4 cBias                = fmtInfo.valueMin;
        int numLevels                  = deLog2Floor32(de::max(m_width, m_height)) + 1;

        // Create textures.
        m_gradientTex = new glu::Texture2DArray(m_context.getRenderContext(), m_format, m_width, m_height, m_numLayers);
        m_gridTex     = new glu::Texture2DArray(m_context.getRenderContext(), m_format, m_width, m_height, m_numLayers);

        // Fill first gradient texture.
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            tcu::Vec4 gMin = tcu::Vec4(-0.5f, -0.5f, -0.5f, 2.0f) * cScale + cBias;
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
    }
    catch (...)
    {
        // Clean up to save memory.
        Texture2DArrayShadowCase::deinit();
        throw;
    }

    // Compute cases.
    {
        const float refInRangeUpper     = (m_compareFunc == GL_EQUAL || m_compareFunc == GL_NOTEQUAL) ? 1.0f : 0.5f;
        const float refInRangeLower     = (m_compareFunc == GL_EQUAL || m_compareFunc == GL_NOTEQUAL) ? 0.0f : 0.5f;
        const float refOutOfBoundsUpper = 1.1f; // !< lookup function should clamp values to [0, 1] range
        const float refOutOfBoundsLower = -0.1f;

        const struct
        {
            int texNdx;
            float ref;
            float lodX;
            float lodY;
            float oX;
            float oY;
        } cases[] = {
            {0, refInRangeUpper, 1.6f, 2.9f, -1.0f, -2.7f},
            {0, refInRangeLower, -2.0f, -1.35f, -0.2f, 0.7f},
            {1, refInRangeUpper, 0.14f, 0.275f, -1.5f, -1.1f},
            {1, refInRangeLower, -0.92f, -2.64f, 0.4f, -0.1f},
            {1, refOutOfBoundsUpper, -0.49f, -0.22f, 0.45f, 0.97f},
            {1, refOutOfBoundsLower, -0.85f, 0.75f, 0.25f, 0.61f},
        };

        const float viewportW = (float)de::min<int>(TEX2D_VIEWPORT_WIDTH, m_context.getRenderTarget().getWidth());
        const float viewportH = (float)de::min<int>(TEX2D_VIEWPORT_HEIGHT, m_context.getRenderTarget().getHeight());

        const float minLayer = -0.5f;
        const float maxLayer = (float)m_numLayers;

        for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); caseNdx++)
        {
            const glu::Texture2DArray *tex = cases[caseNdx].texNdx > 0 ? m_gridTex : m_gradientTex;
            const float ref                = cases[caseNdx].ref;
            const float lodX               = cases[caseNdx].lodX;
            const float lodY               = cases[caseNdx].lodY;
            const float oX                 = cases[caseNdx].oX;
            const float oY                 = cases[caseNdx].oY;
            const float sX                 = deFloatExp2(lodX) * viewportW / float(tex->getRefTexture().getWidth());
            const float sY                 = deFloatExp2(lodY) * viewportH / float(tex->getRefTexture().getHeight());

            m_cases.push_back(FilterCase(tex, ref, tcu::Vec3(oX, oY, minLayer), tcu::Vec3(oX + sX, oY + sY, maxLayer)));
        }
    }

    m_caseNdx = 0;
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
}

void Texture2DArrayShadowCase::deinit(void)
{
    delete m_gradientTex;
    delete m_gridTex;

    m_gradientTex = nullptr;
    m_gridTex     = nullptr;

    m_renderer.clear();
    m_cases.clear();
}

Texture2DArrayShadowCase::IterateResult Texture2DArrayShadowCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const RandomViewport viewport(m_context.getRenderTarget(), TEX2D_VIEWPORT_WIDTH, TEX2D_VIEWPORT_HEIGHT,
                                  deStringHash(getName()) ^ deInt32Hash(m_caseNdx));
    const FilterCase &curCase = m_cases[m_caseNdx];
    const tcu::ScopedLogSection section(m_testCtx.getLog(), string("Test") + de::toString(m_caseNdx),
                                        string("Test ") + de::toString(m_caseNdx));
    ReferenceParams sampleParams(TEXTURETYPE_2D_ARRAY);
    tcu::Surface rendered(viewport.width, viewport.height);

    const float texCoord[] = {
        curCase.minCoord.x(), curCase.minCoord.y(), curCase.minCoord.z(),
        curCase.minCoord.x(), curCase.maxCoord.y(), (curCase.minCoord.z() + curCase.maxCoord.z()) / 2.0f,
        curCase.maxCoord.x(), curCase.minCoord.y(), (curCase.minCoord.z() + curCase.maxCoord.z()) / 2.0f,
        curCase.maxCoord.x(), curCase.maxCoord.y(), curCase.maxCoord.z()};

    if (viewport.width < TEX2D_MIN_VIEWPORT_WIDTH || viewport.height < TEX2D_MIN_VIEWPORT_HEIGHT)
        throw tcu::NotSupportedError("Too small render target", "", __FILE__, __LINE__);

    // Setup params for reference.
    sampleParams.sampler         = glu::mapGLSampler(m_wrapS, m_wrapT, m_minFilter, m_magFilter);
    sampleParams.sampler.compare = glu::mapGLCompareFunc(m_compareFunc);
    sampleParams.samplerType     = SAMPLERTYPE_SHADOW;
    sampleParams.lodMode         = LODMODE_EXACT;
    sampleParams.ref             = curCase.ref;

    m_testCtx.getLog() << TestLog::Message << "Compare reference value =  " << sampleParams.ref << "\n"
                       << "Texture coordinates: " << curCase.minCoord << " -> " << curCase.maxCoord
                       << TestLog::EndMessage;

    gl.bindTexture(GL_TEXTURE_2D_ARRAY, curCase.texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, m_magFilter);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, m_wrapS);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, m_wrapT);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, m_compareFunc);

    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);
    m_renderer.renderQuad(0, &texCoord[0], sampleParams);
    glu::readPixels(m_context.getRenderContext(), viewport.x, viewport.y, rendered.getAccess());

    {
        const tcu::PixelFormat pixelFormat = m_context.getRenderTarget().getPixelFormat();
        tcu::LodPrecision lodPrecision;
        tcu::TexComparePrecision texComparePrecision;

        lodPrecision.derivateBits         = 18;
        lodPrecision.lodBits              = 6;
        texComparePrecision.coordBits     = tcu::IVec3(20, 20, 20);
        texComparePrecision.uvwBits       = tcu::IVec3(7, 7, 7);
        texComparePrecision.pcfBits       = 5;
        texComparePrecision.referenceBits = 16;
        texComparePrecision.resultBits    = pixelFormat.redBits - 1;

        const bool isHighQuality =
            verifyTexCompareResult(m_testCtx, rendered.getAccess(), curCase.texture->getRefTexture(), &texCoord[0],
                                   sampleParams, texComparePrecision, lodPrecision, pixelFormat);

        if (!isHighQuality)
        {
            m_testCtx.getLog() << TestLog::Message
                               << "Warning: Verification assuming high-quality PCF filtering failed."
                               << TestLog::EndMessage;

            lodPrecision.lodBits        = 4;
            texComparePrecision.uvwBits = tcu::IVec3(4, 4, 4);
            texComparePrecision.pcfBits = 0;

            const bool isOk =
                verifyTexCompareResult(m_testCtx, rendered.getAccess(), curCase.texture->getRefTexture(), &texCoord[0],
                                       sampleParams, texComparePrecision, lodPrecision, pixelFormat);

            if (!isOk)
            {
                m_testCtx.getLog()
                    << TestLog::Message
                    << "ERROR: Verification against low precision requirements failed, failing test case."
                    << TestLog::EndMessage;
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image verification failed");
            }
            else if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING, "Low-quality result");
        }
    }

    m_caseNdx += 1;
    return m_caseNdx < (int)m_cases.size() ? CONTINUE : STOP;
}

TextureShadowTests::TextureShadowTests(Context &context)
    : TestCaseGroup(context, "shadow", "Shadow texture lookup tests")
{
}

TextureShadowTests::~TextureShadowTests(void)
{
}

void TextureShadowTests::init(void)
{
    static const struct
    {
        const char *name;
        uint32_t format;
    } formats[] = {{"depth_component16", GL_DEPTH_COMPONENT16},
                   {"depth_component32f", GL_DEPTH_COMPONENT32F},
                   {"depth24_stencil8", GL_DEPTH24_STENCIL8}};

    static const struct
    {
        const char *name;
        uint32_t minFilter;
        uint32_t magFilter;
    } filters[] = {{"nearest", GL_NEAREST, GL_NEAREST},
                   {"linear", GL_LINEAR, GL_LINEAR},
                   {"nearest_mipmap_nearest", GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR},
                   {"linear_mipmap_nearest", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
                   {"nearest_mipmap_linear", GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR},
                   {"linear_mipmap_linear", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}};

    static const struct
    {
        const char *name;
        uint32_t func;
    } compareFuncs[] = {
        {"less_or_equal", GL_LEQUAL}, {"greater_or_equal", GL_GEQUAL}, {"less", GL_LESS},     {"greater", GL_GREATER},
        {"equal", GL_EQUAL},          {"not_equal", GL_NOTEQUAL},      {"always", GL_ALWAYS}, {"never", GL_NEVER}};

    // 2D cases.
    {
        tcu::TestCaseGroup *group2D = new tcu::TestCaseGroup(m_testCtx, "2d", "2D texture shadow lookup tests");
        addChild(group2D);

        for (int filterNdx = 0; filterNdx < DE_LENGTH_OF_ARRAY(filters); filterNdx++)
        {
            tcu::TestCaseGroup *filterGroup = new tcu::TestCaseGroup(m_testCtx, filters[filterNdx].name, "");
            group2D->addChild(filterGroup);

            for (int compareNdx = 0; compareNdx < DE_LENGTH_OF_ARRAY(compareFuncs); compareNdx++)
            {
                for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
                {
                    uint32_t minFilter   = filters[filterNdx].minFilter;
                    uint32_t magFilter   = filters[filterNdx].magFilter;
                    uint32_t format      = formats[formatNdx].format;
                    uint32_t compareFunc = compareFuncs[compareNdx].func;
                    const uint32_t wrapS = GL_REPEAT;
                    const uint32_t wrapT = GL_REPEAT;
                    const int width      = 32;
                    const int height     = 64;
                    string name          = string(compareFuncs[compareNdx].name) + "_" + formats[formatNdx].name;

                    filterGroup->addChild(new Texture2DShadowCase(m_context, name.c_str(), "", minFilter, magFilter,
                                                                  wrapS, wrapT, format, width, height, compareFunc));
                }
            }
        }
    }

    // Cubemap cases.
    {
        tcu::TestCaseGroup *groupCube =
            new tcu::TestCaseGroup(m_testCtx, "cube", "Cube map texture shadow lookup tests");
        addChild(groupCube);

        for (int filterNdx = 0; filterNdx < DE_LENGTH_OF_ARRAY(filters); filterNdx++)
        {
            tcu::TestCaseGroup *filterGroup = new tcu::TestCaseGroup(m_testCtx, filters[filterNdx].name, "");
            groupCube->addChild(filterGroup);

            for (int compareNdx = 0; compareNdx < DE_LENGTH_OF_ARRAY(compareFuncs); compareNdx++)
            {
                for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
                {
                    uint32_t minFilter   = filters[filterNdx].minFilter;
                    uint32_t magFilter   = filters[filterNdx].magFilter;
                    uint32_t format      = formats[formatNdx].format;
                    uint32_t compareFunc = compareFuncs[compareNdx].func;
                    const uint32_t wrapS = GL_REPEAT;
                    const uint32_t wrapT = GL_REPEAT;
                    const int size       = 32;
                    string name          = string(compareFuncs[compareNdx].name) + "_" + formats[formatNdx].name;

                    filterGroup->addChild(new TextureCubeShadowCase(m_context, name.c_str(), "", minFilter, magFilter,
                                                                    wrapS, wrapT, format, size, compareFunc));
                }
            }
        }
    }

    // 2D array cases.
    {
        tcu::TestCaseGroup *group2DArray =
            new tcu::TestCaseGroup(m_testCtx, "2d_array", "2D texture array shadow lookup tests");
        addChild(group2DArray);

        for (int filterNdx = 0; filterNdx < DE_LENGTH_OF_ARRAY(filters); filterNdx++)
        {
            tcu::TestCaseGroup *filterGroup = new tcu::TestCaseGroup(m_testCtx, filters[filterNdx].name, "");
            group2DArray->addChild(filterGroup);

            for (int compareNdx = 0; compareNdx < DE_LENGTH_OF_ARRAY(compareFuncs); compareNdx++)
            {
                for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
                {
                    uint32_t minFilter   = filters[filterNdx].minFilter;
                    uint32_t magFilter   = filters[filterNdx].magFilter;
                    uint32_t format      = formats[formatNdx].format;
                    uint32_t compareFunc = compareFuncs[compareNdx].func;
                    const uint32_t wrapS = GL_REPEAT;
                    const uint32_t wrapT = GL_REPEAT;
                    const int width      = 32;
                    const int height     = 64;
                    const int numLayers  = 8;
                    string name          = string(compareFuncs[compareNdx].name) + "_" + formats[formatNdx].name;

                    filterGroup->addChild(new Texture2DArrayShadowCase(m_context, name.c_str(), "", minFilter,
                                                                       magFilter, wrapS, wrapT, format, width, height,
                                                                       numLayers, compareFunc));
                }
            }
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
