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
 * \brief Texture swizzle tests.
 *//*--------------------------------------------------------------------*/

#include "es3fTextureSwizzleTests.hpp"
#include "glsTextureTestUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluTexture.hpp"
#include "gluRenderContext.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRenderTarget.hpp"
#include "deString.h"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::string;
using std::vector;
using tcu::TestLog;
using namespace deqp::gls;
using namespace deqp::gls::TextureTestUtil;
using namespace glu::TextureTestUtil;

static int swizzle(const tcu::RGBA &c, uint32_t swz)
{
    switch (swz)
    {
    case GL_RED:
        return c.getRed();
    case GL_GREEN:
        return c.getGreen();
    case GL_BLUE:
        return c.getBlue();
    case GL_ALPHA:
        return c.getAlpha();
    case GL_ZERO:
        return 0;
    case GL_ONE:
        return (1 << 8) - 1;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

static void swizzle(tcu::Surface &surface, uint32_t swzR, uint32_t swzG, uint32_t swzB, uint32_t swzA)
{
    for (int y = 0; y < surface.getHeight(); y++)
    {
        for (int x = 0; x < surface.getWidth(); x++)
        {
            tcu::RGBA p = surface.getPixel(x, y);
            surface.setPixel(x, y, tcu::RGBA(swizzle(p, swzR), swizzle(p, swzG), swizzle(p, swzB), swizzle(p, swzA)));
        }
    }
}

class Texture2DSwizzleCase : public TestCase
{
public:
    Texture2DSwizzleCase(Context &context, const char *name, const char *description, uint32_t internalFormat,
                         uint32_t format, uint32_t dataType, uint32_t swizzleR, uint32_t swizzleG, uint32_t swizzleB,
                         uint32_t swizzleA);
    ~Texture2DSwizzleCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture2DSwizzleCase(const Texture2DSwizzleCase &other);
    Texture2DSwizzleCase &operator=(const Texture2DSwizzleCase &other);

    uint32_t m_internalFormat;
    uint32_t m_format;
    uint32_t m_dataType;
    uint32_t m_swizzleR;
    uint32_t m_swizzleG;
    uint32_t m_swizzleB;
    uint32_t m_swizzleA;

    glu::Texture2D *m_texture;
    TextureRenderer m_renderer;
};

Texture2DSwizzleCase::Texture2DSwizzleCase(Context &context, const char *name, const char *description,
                                           uint32_t internalFormat, uint32_t format, uint32_t dataType,
                                           uint32_t swizzleR, uint32_t swizzleG, uint32_t swizzleB, uint32_t swizzleA)
    : TestCase(context, name, description)
    , m_internalFormat(internalFormat)
    , m_format(format)
    , m_dataType(dataType)
    , m_swizzleR(swizzleR)
    , m_swizzleG(swizzleG)
    , m_swizzleB(swizzleB)
    , m_swizzleA(swizzleA)
    , m_texture(nullptr)
    , m_renderer(context.getRenderContext(), context.getTestContext().getLog(), glu::GLSL_VERSION_300_ES,
                 glu::PRECISION_HIGHP)
{
}

Texture2DSwizzleCase::~Texture2DSwizzleCase(void)
{
    deinit();
}

void Texture2DSwizzleCase::init(void)
{
    int width  = de::min(128, m_context.getRenderContext().getRenderTarget().getWidth());
    int height = de::min(128, m_context.getRenderContext().getRenderTarget().getHeight());

    m_texture = (m_internalFormat == m_format) ?
                    new glu::Texture2D(m_context.getRenderContext(), m_format, m_dataType, width, height) :
                    new glu::Texture2D(m_context.getRenderContext(), m_internalFormat, width, height);

    tcu::TextureFormatInfo spec = tcu::getTextureFormatInfo(m_texture->getRefTexture().getFormat());

    // Fill level 0.
    m_texture->getRefTexture().allocLevel(0);
    tcu::fillWithComponentGradients(m_texture->getRefTexture().getLevel(0), spec.valueMin, spec.valueMax);
}

void Texture2DSwizzleCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

Texture2DSwizzleCase::IterateResult Texture2DSwizzleCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    TestLog &log             = m_testCtx.getLog();
    RandomViewport viewport(m_context.getRenderContext().getRenderTarget(), m_texture->getRefTexture().getWidth(),
                            m_texture->getRefTexture().getHeight(), deStringHash(getName()));
    tcu::Surface renderedFrame(viewport.width, viewport.height);
    tcu::Surface referenceFrame(viewport.width, viewport.height);
    tcu::RGBA threshold = m_context.getRenderTarget().getPixelFormat().getColorThreshold() + tcu::RGBA(1, 1, 1, 1);
    vector<float> texCoord;
    ReferenceParams renderParams(TEXTURETYPE_2D);
    tcu::TextureFormatInfo spec = tcu::getTextureFormatInfo(m_texture->getRefTexture().getFormat());

    renderParams.samplerType = getSamplerType(m_texture->getRefTexture().getFormat());
    renderParams.sampler     = tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
                                            tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::NEAREST, tcu::Sampler::NEAREST);
    renderParams.colorScale  = spec.lookupScale;
    renderParams.colorBias   = spec.lookupBias;

    computeQuadTexCoord2D(texCoord, tcu::Vec2(0.0f, 0.0f), tcu::Vec2(1.0f, 1.0f));

    // Setup base viewport.
    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

    // Upload texture data to GL.
    m_texture->upload();

    // Bind to unit 0.
    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_2D, m_texture->getGLTexture());

    // Setup nearest neighbor filtering and clamp-to-edge.
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Setup texture swizzle.
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, m_swizzleR);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, m_swizzleG);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, m_swizzleB);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, m_swizzleA);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Set texturing state");

    // Draw.
    m_renderer.renderQuad(0, &texCoord[0], renderParams);
    glu::readPixels(m_context.getRenderContext(), viewport.x, viewport.y, renderedFrame.getAccess());

    // Compute reference
    {
        const tcu::PixelFormat pixelFormat = m_context.getRenderTarget().getPixelFormat();

        // Do initial rendering to RGBA8 in order to keep alpha
        sampleTexture(tcu::SurfaceAccess(referenceFrame, tcu::PixelFormat(8, 8, 8, 8)), m_texture->getRefTexture(),
                      &texCoord[0], renderParams);

        // Swizzle channels
        swizzle(referenceFrame, m_swizzleR, m_swizzleG, m_swizzleB, m_swizzleA);

        // Convert to destination format
        if (pixelFormat != tcu::PixelFormat(8, 8, 8, 8))
        {
            for (int y = 0; y < referenceFrame.getHeight(); y++)
            {
                for (int x = 0; x < referenceFrame.getWidth(); x++)
                {
                    tcu::RGBA p = referenceFrame.getPixel(x, y);
                    referenceFrame.setPixel(x, y, pixelFormat.convertColor(p));
                }
            }
        }
    }

    // Compare and log.
    bool isOk = compareImages(log, referenceFrame, renderedFrame, threshold);

    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                            isOk ? "Pass" : "Image comparison failed");

    return STOP;
}

TextureSwizzleTests::TextureSwizzleTests(Context &context) : TestCaseGroup(context, "swizzle", "Texture Swizzle Tests")
{
}

TextureSwizzleTests::~TextureSwizzleTests(void)
{
}

void TextureSwizzleTests::init(void)
{
    static const struct
    {
        const char *name;
        uint32_t internalFormat;
        uint32_t format;
        uint32_t dataType;
    } formats[] = {{"alpha", GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE},
                   {"luminance", GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE},
                   {"luminance_alpha", GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE},
                   {"red", GL_R8, GL_RED, GL_UNSIGNED_BYTE},
                   {"rg", GL_RG8, GL_RG, GL_UNSIGNED_BYTE},
                   {"rgb", GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE},
                   {"rgba", GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}};

    static const struct
    {
        const char *name;
        uint32_t channel;
    } channels[] = {{"r", GL_TEXTURE_SWIZZLE_R},
                    {"g", GL_TEXTURE_SWIZZLE_G},
                    {"b", GL_TEXTURE_SWIZZLE_B},
                    {"a", GL_TEXTURE_SWIZZLE_A}};

    static const struct
    {
        const char *name;
        uint32_t swizzle;
    } swizzles[] = {{"red", GL_RED},     {"green", GL_GREEN}, {"blue", GL_BLUE},
                    {"alpha", GL_ALPHA}, {"zero", GL_ZERO},   {"one", GL_ONE}};

    static const struct
    {
        const char *name;
        uint32_t swzR;
        uint32_t swzG;
        uint32_t swzB;
        uint32_t swzA;
    } swizzleCases[] = {{"all_red", GL_RED, GL_RED, GL_RED, GL_RED},
                        {"all_green", GL_GREEN, GL_GREEN, GL_GREEN, GL_GREEN},
                        {"all_blue", GL_BLUE, GL_BLUE, GL_BLUE, GL_BLUE},
                        {"all_alpha", GL_ALPHA, GL_ALPHA, GL_ALPHA, GL_ALPHA},
                        {"all_zero", GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO},
                        {"all_one", GL_ONE, GL_ONE, GL_ONE, GL_ONE},
                        {"bgra", GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA},
                        {"abgr", GL_ALPHA, GL_BLUE, GL_GREEN, GL_RED},
                        {"one_one_red_green", GL_ONE, GL_ONE, GL_RED, GL_GREEN}};

    static const uint32_t defaultSwizzles[] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA};

    // All swizzles applied to each channel.
    tcu::TestCaseGroup *singleChannelGroup =
        new tcu::TestCaseGroup(m_testCtx, "single_channel", "Single-channel swizzle");
    addChild(singleChannelGroup);
    for (int chanNdx = 0; chanNdx < DE_LENGTH_OF_ARRAY(channels); chanNdx++)
    {
        for (int swzNdx = 0; swzNdx < DE_LENGTH_OF_ARRAY(swizzles); swzNdx++)
        {
            if (swizzles[swzNdx].swizzle == defaultSwizzles[chanNdx])
                continue; // No need to test default case.

            string name   = string(channels[chanNdx].name) + "_" + swizzles[swzNdx].name;
            uint32_t swz  = swizzles[swzNdx].swizzle;
            uint32_t swzR = (chanNdx == 0) ? swz : defaultSwizzles[0];
            uint32_t swzG = (chanNdx == 1) ? swz : defaultSwizzles[1];
            uint32_t swzB = (chanNdx == 2) ? swz : defaultSwizzles[2];
            uint32_t swzA = (chanNdx == 3) ? swz : defaultSwizzles[3];

            singleChannelGroup->addChild(new Texture2DSwizzleCase(m_context, name.c_str(), "Single-channel swizzle",
                                                                  GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, swzR, swzG, swzB,
                                                                  swzA));
        }
    }

    // Swizzles for all formats.
    tcu::TestCaseGroup *multiChannelGroup = new tcu::TestCaseGroup(m_testCtx, "multi_channel", "Multi-channel swizzle");
    addChild(multiChannelGroup);
    for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(formats); fmtNdx++)
    {
        for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(swizzleCases); caseNdx++)
        {
            string name        = string(formats[fmtNdx].name) + "_" + swizzleCases[caseNdx].name;
            uint32_t swzR      = swizzleCases[caseNdx].swzR;
            uint32_t swzG      = swizzleCases[caseNdx].swzG;
            uint32_t swzB      = swizzleCases[caseNdx].swzB;
            uint32_t swzA      = swizzleCases[caseNdx].swzA;
            uint32_t intFormat = formats[fmtNdx].internalFormat;
            uint32_t format    = formats[fmtNdx].format;
            uint32_t dataType  = formats[fmtNdx].dataType;

            multiChannelGroup->addChild(new Texture2DSwizzleCase(m_context, name.c_str(), "Multi-channel swizzle",
                                                                 intFormat, format, dataType, swzR, swzG, swzB, swzA));
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
