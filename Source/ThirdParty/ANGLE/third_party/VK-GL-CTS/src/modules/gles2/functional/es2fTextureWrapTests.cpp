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
 * \brief Texture wrap mode tests.
 *//*--------------------------------------------------------------------*/

#include "es2fTextureWrapTests.hpp"
#include "glsTextureTestUtil.hpp"
#include "gluTexture.hpp"
#include "gluStrUtil.hpp"
#include "gluTextureUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

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
    VIEWPORT_WIDTH  = 256,
    VIEWPORT_HEIGHT = 256
};

class TextureWrapCase : public tcu::TestCase
{
public:
    TextureWrapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                    const char *name, const char *description, uint32_t format, uint32_t dataType, uint32_t wrapS,
                    uint32_t wrapT, uint32_t minFilter, uint32_t magFilter, int width, int height,
                    bool enableRelaxedRef = false);
    TextureWrapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                    const char *name, const char *description, uint32_t wrapS, uint32_t wrapT, uint32_t minFilter,
                    uint32_t magFilter, const std::vector<std::string> &filenames, bool enableRelaxedRef = false);
    ~TextureWrapCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    TextureWrapCase(const TextureWrapCase &other);
    TextureWrapCase &operator=(const TextureWrapCase &other);

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    uint32_t m_format;
    uint32_t m_dataType;
    uint32_t m_wrapS;
    uint32_t m_wrapT;
    uint32_t m_minFilter;
    uint32_t m_magFilter;

    int m_width;
    int m_height;
    std::vector<std::string> m_filenames;

    glu::Texture2D *m_texture;
    TextureRenderer m_renderer;

    bool m_enableRelaxedRef;
};

TextureWrapCase::TextureWrapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                 const glu::ContextInfo &ctxInfo, const char *name, const char *description,
                                 uint32_t format, uint32_t dataType, uint32_t wrapS, uint32_t wrapT, uint32_t minFilter,
                                 uint32_t magFilter, int width, int height, bool enableRelaxedRef)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(ctxInfo)
    , m_format(format)
    , m_dataType(dataType)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_width(width)
    , m_height(height)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_100_ES, glu::PRECISION_MEDIUMP)
    , m_enableRelaxedRef(enableRelaxedRef)
{
}

TextureWrapCase::TextureWrapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                 const glu::ContextInfo &ctxInfo, const char *name, const char *description,
                                 uint32_t wrapS, uint32_t wrapT, uint32_t minFilter, uint32_t magFilter,
                                 const std::vector<std::string> &filenames, bool enableRelaxedRef)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(ctxInfo)
    , m_format(GL_NONE)
    , m_dataType(GL_NONE)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_width(0)
    , m_height(0)
    , m_filenames(filenames)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_100_ES, glu::PRECISION_MEDIUMP)
    , m_enableRelaxedRef(enableRelaxedRef)
{
}

TextureWrapCase::~TextureWrapCase(void)
{
    deinit();
}

void TextureWrapCase::init(void)
{
    if (!m_filenames.empty())
    {
        DE_ASSERT(m_width == 0 && m_height == 0 && m_format == GL_NONE && m_dataType == GL_NONE);

        m_texture = glu::Texture2D::create(m_renderCtx, m_renderCtxInfo, m_testCtx.getArchive(),
                                           (int)m_filenames.size(), m_filenames);
        m_width   = m_texture->getRefTexture().getWidth();
        m_height  = m_texture->getRefTexture().getHeight();
    }
    else
    {
        m_texture = new Texture2D(m_renderCtx, m_format, m_dataType, m_width, m_height);

        // Fill level 0.
        m_texture->getRefTexture().allocLevel(0);
        if (m_wrapS == GL_REPEAT || m_wrapT == GL_REPEAT)
        {
            // If run in repeat mode, use conical style texture to avoid edge sample result have a huge difference when coordinate offset in allow range.
            tcu::fillWithComponentGradients3(m_texture->getRefTexture().getLevel(0),
                                             tcu::Vec4(-0.5f, -0.5f, -0.5f, 1.5f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f));
        }
        else
        {
            tcu::fillWithComponentGradients(m_texture->getRefTexture().getLevel(0),
                                            tcu::Vec4(-0.5f, -0.5f, -0.5f, 1.5f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f));
        }

        m_texture->upload();
    }
}

void TextureWrapCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

TextureWrapCase::IterateResult TextureWrapCase::iterate(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    TestLog &log             = m_testCtx.getLog();
    RandomViewport viewport(m_renderCtx.getRenderTarget(), VIEWPORT_WIDTH, VIEWPORT_HEIGHT, deStringHash(getName()));
    tcu::Surface renderedFrame(viewport.width, viewport.height);
    tcu::Surface referenceFrame(viewport.width, viewport.height);
    bool isCompressedTex = !m_filenames.empty();
    ReferenceParams refParams(TEXTURETYPE_2D);
    int leftWidth  = viewport.width / 2;
    int rightWidth = viewport.width - leftWidth;
    vector<float> texCoord;

    tcu::RGBA threshold;
    if (m_texture->getRefTexture().getFormat().type == tcu::TextureFormat::UNORM_SHORT_4444 ||
        m_texture->getRefTexture().getFormat().type == tcu::TextureFormat::UNSIGNED_SHORT_4444)
    {
        threshold = tcu::PixelFormat(4, 4, 4, 4).getColorThreshold() + tcu::RGBA(1, 1, 1, 1);
    }
    else
    {
        threshold = m_renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() +
                    (isCompressedTex ? tcu::RGBA(7, 7, 7, 7) : tcu::RGBA(3, 3, 3, 3));
    }

    // Bind to unit 0.
    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_2D, m_texture->getGLTexture());

    // Setup filtering and wrap modes.
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapS);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapT);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_magFilter);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Set texturing state");

    // Parameters for reference images.
    refParams.sampler = mapGLSampler(m_wrapS, m_wrapT, m_minFilter, m_magFilter);
    refParams.lodMode = LODMODE_EXACT;

    // Left: minification
    {
        gl.viewport(viewport.x, viewport.y, leftWidth, viewport.height);

        computeQuadTexCoord2D(texCoord, tcu::Vec2(-1.5f, -3.0f), tcu::Vec2(1.5f, 2.5f));

        m_renderer.renderQuad(0, &texCoord[0], refParams);
        glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());

        sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat(), 0, 0,
                                         leftWidth, viewport.height),
                      m_texture->getRefTexture(), &texCoord[0], refParams);
    }

    // Right: magnification
    {
        gl.viewport(viewport.x + leftWidth, viewport.y, rightWidth, viewport.height);

        computeQuadTexCoord2D(texCoord, tcu::Vec2(-0.5f, 0.75f), tcu::Vec2(0.25f, 1.25f));

        m_renderer.renderQuad(0, &texCoord[0], refParams);
        glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());

        sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat(), leftWidth, 0,
                                         rightWidth, viewport.height),
                      m_texture->getRefTexture(), &texCoord[0], refParams);
    }

    // Compare and log.
    bool isOk = compareImages(log, referenceFrame, renderedFrame, threshold);

    if ((isOk == false) && m_enableRelaxedRef && m_renderer.getTexCoordPrecision() != PRECISION_HIGHP)
    {
        refParams.float16TexCoord = true;
        // Left: minification
        {
            computeQuadTexCoord2D(texCoord, tcu::Vec2(-1.5f, -3.0f), tcu::Vec2(1.5f, 2.5f));
            sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat(), 0, 0,
                                             leftWidth, viewport.height),
                          m_texture->getRefTexture(), &texCoord[0], refParams);
        }

        // Right: magnification
        {
            computeQuadTexCoord2D(texCoord, tcu::Vec2(-0.5f, 0.75f), tcu::Vec2(0.25f, 1.25f));
            sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat(), leftWidth,
                                             0, rightWidth, viewport.height),
                          m_texture->getRefTexture(), &texCoord[0], refParams);
        }

        isOk |= compareImages(log, referenceFrame, renderedFrame, threshold);
    }

    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                            isOk ? "Pass" : "Image comparison failed");

    return STOP;
}

TextureWrapTests::TextureWrapTests(Context &context) : TestCaseGroup(context, "wrap", "Wrap Mode Tests")
{
}

TextureWrapTests::~TextureWrapTests(void)
{
}

void TextureWrapTests::init(void)
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
    } filteringModes[] = {{"nearest", GL_NEAREST}, {"linear", GL_LINEAR}};

    static const struct
    {
        const char *name;
        int width;
        int height;
    } sizes[] = {{"pot", 64, 128}, {"npot", 63, 112}};

    static const struct
    {
        const char *name;
        uint32_t format;
        uint32_t dataType;
    } formats[] = {
        {"rgba8888", GL_RGBA, GL_UNSIGNED_BYTE},
        {"rgb888", GL_RGB, GL_UNSIGNED_BYTE},
        {"rgba4444", GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},
        {"l8", GL_LUMINANCE, GL_UNSIGNED_BYTE},
    };

#define FOR_EACH(ITERATOR, ARRAY, BODY)                                      \
    for (int ITERATOR = 0; ITERATOR < DE_LENGTH_OF_ARRAY(ARRAY); ITERATOR++) \
    BODY

    FOR_EACH(
        wrapS, wrapModes,
        FOR_EACH(
            wrapT, wrapModes,
            FOR_EACH(filter, filteringModes,
                     FOR_EACH(size, sizes, FOR_EACH(format, formats, {
                                  bool is_clamp_clamp            = (wrapModes[wrapS].mode == GL_CLAMP_TO_EDGE &&
                                                         wrapModes[wrapT].mode == GL_CLAMP_TO_EDGE);
                                  bool is_repeat_mirror          = (wrapModes[wrapS].mode == GL_REPEAT &&
                                                           wrapModes[wrapT].mode == GL_MIRRORED_REPEAT);
                                  bool enableRelaxedPrecisionRef = wrapModes[wrapS].mode == GL_REPEAT ||
                                                                   wrapModes[wrapT].mode == GL_REPEAT ||
                                                                   wrapModes[wrapS].mode == GL_MIRRORED_REPEAT ||
                                                                   wrapModes[wrapT].mode == GL_MIRRORED_REPEAT;

                                  if (!is_clamp_clamp && !is_repeat_mirror && format != 0)
                                      continue; // Use other format varants with clamp_clamp & repeat_mirror pair only.

                                  if (!is_clamp_clamp &&
                                      (!deIsPowerOfTwo32(sizes[size].width) || !deIsPowerOfTwo32(sizes[size].height)))
                                      continue; // Not supported as described in Spec section 3.8.2.

                                  string name = string("") + wrapModes[wrapS].name + "_" + wrapModes[wrapT].name + "_" +
                                                filteringModes[filter].name + "_" + sizes[size].name + "_" +
                                                formats[format].name;
                                  addChild(new TextureWrapCase(
                                      m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.c_str(),
                                      "", formats[format].format, formats[format].dataType, wrapModes[wrapS].mode,
                                      wrapModes[wrapT].mode, filteringModes[filter].mode, filteringModes[filter].mode,
                                      sizes[size].width, sizes[size].height, enableRelaxedPrecisionRef));
                              })))))

    // Power-of-two ETC1 texture
    std::vector<std::string> potFilenames;
    potFilenames.push_back("data/etc1/photo_helsinki_mip_0.pkm");

    FOR_EACH(wrapS, wrapModes,
             FOR_EACH(wrapT, wrapModes, FOR_EACH(filter, filteringModes, {
                          bool enableRelaxedPrecisionRef = wrapModes[wrapS].mode == GL_REPEAT ||
                                                           wrapModes[wrapT].mode == GL_REPEAT ||
                                                           wrapModes[wrapS].mode == GL_MIRRORED_REPEAT ||
                                                           wrapModes[wrapT].mode == GL_MIRRORED_REPEAT;

                          string name = string("") + wrapModes[wrapS].name + "_" + wrapModes[wrapT].name + "_" +
                                        filteringModes[filter].name + "_pot_etc1";
                          addChild(new TextureWrapCase(
                              m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.c_str(), "",
                              wrapModes[wrapS].mode, wrapModes[wrapT].mode, filteringModes[filter].mode,
                              filteringModes[filter].mode, potFilenames, enableRelaxedPrecisionRef));
                      })))

    std::vector<std::string> npotFilenames;
    npotFilenames.push_back("data/etc1/photo_helsinki_113x89.pkm");

    // NPOT ETC1 texture
    for (int filter = 0; filter < DE_LENGTH_OF_ARRAY(filteringModes); filter++)
    {
        string name = string("clamp_clamp_") + filteringModes[filter].name + "_npot_etc1";
        addChild(new TextureWrapCase(m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.c_str(),
                                     "", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, filteringModes[filter].mode,
                                     filteringModes[filter].mode, npotFilenames));
    }
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
