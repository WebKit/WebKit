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
 * \brief Texture format tests.
 *
 * Constants:
 *  + nearest-neighbor filtering
 *  + no mipmaps
 *  + full texture coordinate range (but not outside) tested
 *  + accessed from fragment shader
 *  + texture unit 0
 *  + named texture object
 *
 * Variables:
 *  + texture format
 *  + texture type: 2D or cubemap
 *//*--------------------------------------------------------------------*/

#include "es2fTextureFormatTests.hpp"
#include "glsTextureTestUtil.hpp"
#include "gluTexture.hpp"
#include "gluStrUtil.hpp"
#include "gluTextureUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "tcuSurfaceAccess.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

#include "deStringUtil.hpp"

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

// Texture2DFormatCase

class Texture2DFormatCase : public tcu::TestCase
{
public:
    Texture2DFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                        const char *description, uint32_t format, uint32_t dataType, int width, int height);
    ~Texture2DFormatCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture2DFormatCase(const Texture2DFormatCase &other);
    Texture2DFormatCase &operator=(const Texture2DFormatCase &other);

    glu::RenderContext &m_renderCtx;

    const uint32_t m_format;
    const uint32_t m_dataType;
    const int m_width;
    const int m_height;

    glu::Texture2D *m_texture;
    TextureRenderer m_renderer;
};

Texture2DFormatCase::Texture2DFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                         const char *description, uint32_t format, uint32_t dataType, int width,
                                         int height)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_format(format)
    , m_dataType(dataType)
    , m_width(width)
    , m_height(height)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_100_ES, glu::PRECISION_MEDIUMP)
{
}

Texture2DFormatCase::~Texture2DFormatCase(void)
{
    deinit();
}

void Texture2DFormatCase::init(void)
{
    TestLog &log                = m_testCtx.getLog();
    tcu::TextureFormat fmt      = glu::mapGLTransferFormat(m_format, m_dataType);
    tcu::TextureFormatInfo spec = tcu::getTextureFormatInfo(fmt);
    std::ostringstream fmtName;

    fmtName << getTextureFormatStr(m_format) << ", " << getTypeStr(m_dataType);

    log << TestLog::Message << "2D texture, " << fmtName.str() << ", " << m_width << "x" << m_height
        << ",\n  fill with " << formatGradient(&spec.valueMin, &spec.valueMax) << " gradient" << TestLog::EndMessage;

    m_texture = new Texture2D(m_renderCtx, m_format, m_dataType, m_width, m_height);

    // Fill level 0.
    m_texture->getRefTexture().allocLevel(0);
    tcu::fillWithComponentGradients(m_texture->getRefTexture().getLevel(0), spec.valueMin, spec.valueMax);
}

void Texture2DFormatCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

Texture2DFormatCase::IterateResult Texture2DFormatCase::iterate(void)
{
    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_renderCtx.getFunctions();
    RandomViewport viewport(m_renderCtx.getRenderTarget(), m_width, m_height, deStringHash(getName()));
    tcu::Surface renderedFrame(viewport.width, viewport.height);
    tcu::Surface referenceFrame(viewport.width, viewport.height);
    tcu::RGBA threshold = m_renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() + tcu::RGBA(1, 1, 1, 1);
    vector<float> texCoord;
    ReferenceParams renderParams(TEXTURETYPE_2D);
    tcu::TextureFormatInfo spec = tcu::getTextureFormatInfo(m_texture->getRefTexture().getFormat());
    const uint32_t wrapS        = GL_CLAMP_TO_EDGE;
    const uint32_t wrapT        = GL_CLAMP_TO_EDGE;
    const uint32_t minFilter    = GL_NEAREST;
    const uint32_t magFilter    = GL_NEAREST;

    renderParams.flags |= RenderParams::LOG_ALL;
    renderParams.samplerType = getSamplerType(m_texture->getRefTexture().getFormat());
    renderParams.sampler     = Sampler(Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE,
                                       Sampler::NEAREST, Sampler::NEAREST);
    renderParams.colorScale  = spec.lookupScale;
    renderParams.colorBias   = spec.lookupBias;

    computeQuadTexCoord2D(texCoord, tcu::Vec2(0.0f, 0.0f), tcu::Vec2(1.0f, 1.0f));

    log << TestLog::Message << "Texture parameters:"
        << "\n  WRAP_S = " << getTextureParameterValueStr(GL_TEXTURE_WRAP_S, wrapS)
        << "\n  WRAP_T = " << getTextureParameterValueStr(GL_TEXTURE_WRAP_T, wrapT)
        << "\n  MIN_FILTER = " << getTextureParameterValueStr(GL_TEXTURE_MIN_FILTER, minFilter)
        << "\n  MAG_FILTER = " << getTextureParameterValueStr(GL_TEXTURE_MAG_FILTER, magFilter) << TestLog::EndMessage;

    // Setup base viewport.
    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

    // Upload texture data to GL.
    m_texture->upload();

    // Bind to unit 0.
    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_2D, m_texture->getGLTexture());

    // Setup nearest neighbor filtering and clamp-to-edge.
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Set texturing state");

    // Draw.
    m_renderer.renderQuad(0, &texCoord[0], renderParams);
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels()");

    // Compute reference.
    sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat()),
                  m_texture->getRefTexture(), &texCoord[0], renderParams);

    // Compare and log.
    bool isOk = compareImages(log, referenceFrame, renderedFrame, threshold);

    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                            isOk ? "Pass" : "Image comparison failed");

    return STOP;
}

// TextureCubeFormatCase

class TextureCubeFormatCase : public tcu::TestCase
{
public:
    TextureCubeFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                          const char *description, uint32_t format, uint32_t dataType, int width, int height);
    ~TextureCubeFormatCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    TextureCubeFormatCase(const TextureCubeFormatCase &other);
    TextureCubeFormatCase &operator=(const TextureCubeFormatCase &other);

    bool testFace(tcu::CubeFace face);

    glu::RenderContext &m_renderCtx;

    const uint32_t m_format;
    const uint32_t m_dataType;
    const int m_width;
    const int m_height;

    glu::TextureCube *m_texture;
    TextureRenderer m_renderer;

    int m_curFace;
    bool m_isOk;
};

TextureCubeFormatCase::TextureCubeFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                             const char *description, uint32_t format, uint32_t dataType, int width,
                                             int height)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_format(format)
    , m_dataType(dataType)
    , m_width(width)
    , m_height(height)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_100_ES, glu::PRECISION_MEDIUMP)
    , m_curFace(0)
    , m_isOk(false)
{
}

TextureCubeFormatCase::~TextureCubeFormatCase(void)
{
    deinit();
}

void TextureCubeFormatCase::init(void)
{
    TestLog &log                = m_testCtx.getLog();
    tcu::TextureFormat fmt      = glu::mapGLTransferFormat(m_format, m_dataType);
    tcu::TextureFormatInfo spec = tcu::getTextureFormatInfo(fmt);
    std::ostringstream fmtName;

    if (m_dataType)
        fmtName << getTextureFormatStr(m_format) << ", " << getTypeStr(m_dataType);
    else
        fmtName << getTextureFormatStr(m_format);

    log << TestLog::Message << "Cube map texture, " << fmtName.str() << ", " << m_width << "x" << m_height
        << ",\n  fill with " << formatGradient(&spec.valueMin, &spec.valueMax) << " gradient" << TestLog::EndMessage;

    DE_ASSERT(m_width == m_height);
    m_texture = m_dataType != GL_NONE ?
                    new TextureCube(m_renderCtx, m_format, m_dataType, m_width) // Implicit internal format.
                    :
                    new TextureCube(m_renderCtx, m_format, m_width); // Explicit internal format.

    // Fill level 0.
    for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
    {
        tcu::Vec4 gMin, gMax;

        switch (face)
        {
        case 0:
            gMin = spec.valueMin.swizzle(0, 1, 2, 3);
            gMax = spec.valueMax.swizzle(0, 1, 2, 3);
            break;
        case 1:
            gMin = spec.valueMin.swizzle(2, 1, 0, 3);
            gMax = spec.valueMax.swizzle(2, 1, 0, 3);
            break;
        case 2:
            gMin = spec.valueMin.swizzle(1, 2, 0, 3);
            gMax = spec.valueMax.swizzle(1, 2, 0, 3);
            break;
        case 3:
            gMin = spec.valueMax.swizzle(0, 1, 2, 3);
            gMax = spec.valueMin.swizzle(0, 1, 2, 3);
            break;
        case 4:
            gMin = spec.valueMax.swizzle(2, 1, 0, 3);
            gMax = spec.valueMin.swizzle(2, 1, 0, 3);
            break;
        case 5:
            gMin = spec.valueMax.swizzle(1, 2, 0, 3);
            gMax = spec.valueMin.swizzle(1, 2, 0, 3);
            break;
        default:
            DE_ASSERT(false);
        }

        m_texture->getRefTexture().allocLevel((tcu::CubeFace)face, 0);
        tcu::fillWithComponentGradients(m_texture->getRefTexture().getLevelFace(0, (tcu::CubeFace)face), gMin, gMax);
    }

    // Upload texture data to GL.
    m_texture->upload();

    // Initialize iteration state.
    m_curFace = 0;
    m_isOk    = true;
}

void TextureCubeFormatCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

bool TextureCubeFormatCase::testFace(tcu::CubeFace face)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    TestLog &log             = m_testCtx.getLog();
    RandomViewport viewport(m_renderCtx.getRenderTarget(), m_width, m_height, deStringHash(getName()) + (uint32_t)face);
    tcu::Surface renderedFrame(viewport.width, viewport.height);
    tcu::Surface referenceFrame(viewport.width, viewport.height);
    tcu::RGBA threshold = m_renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() + tcu::RGBA(1, 1, 1, 1);
    vector<float> texCoord;
    ReferenceParams renderParams(TEXTURETYPE_CUBE);
    tcu::TextureFormatInfo spec = tcu::getTextureFormatInfo(m_texture->getRefTexture().getFormat());

    renderParams.samplerType = getSamplerType(m_texture->getRefTexture().getFormat());
    renderParams.sampler     = Sampler(Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE,
                                       Sampler::NEAREST, Sampler::NEAREST);
    renderParams.sampler.seamlessCubeMap = false;
    renderParams.colorScale              = spec.lookupScale;
    renderParams.colorBias               = spec.lookupBias;

    // Log render info on first face.
    if (face == tcu::CUBEFACE_NEGATIVE_X)
        renderParams.flags |= RenderParams::LOG_ALL;

    computeQuadTexCoordCube(texCoord, face);

    // \todo [2011-10-28 pyry] Image set name / section?
    log << TestLog::Message << face << TestLog::EndMessage;

    // Setup base viewport.
    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

    // Bind to unit 0.
    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_CUBE_MAP, m_texture->getGLTexture());

    // Setup nearest neighbor filtering and clamp-to-edge.
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Set texturing state");

    m_renderer.renderQuad(0, &texCoord[0], renderParams);
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels()");

    // Compute reference.
    sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat()),
                  m_texture->getRefTexture(), &texCoord[0], renderParams);

    // Compare and log.
    return compareImages(log, referenceFrame, renderedFrame, threshold);
}

TextureCubeFormatCase::IterateResult TextureCubeFormatCase::iterate(void)
{
    // Execute test for all faces.
    if (!testFace((tcu::CubeFace)m_curFace))
        m_isOk = false;

    m_curFace += 1;

    if (m_curFace == tcu::CUBEFACE_LAST)
    {
        m_testCtx.setTestResult(m_isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                m_isOk ? "Pass" : "Image comparison failed");
        return STOP;
    }
    else
        return CONTINUE;
}

TextureFormatTests::TextureFormatTests(Context &context) : TestCaseGroup(context, "format", "Texture Format Tests")
{
}

TextureFormatTests::~TextureFormatTests(void)
{
}

// Compressed2DFormatCase

class Compressed2DFormatCase : public tcu::TestCase
{
public:
    Compressed2DFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                           const glu::ContextInfo &renderCtxInfo, const char *name, const char *description,
                           const std::vector<std::string> &filenames);
    ~Compressed2DFormatCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Compressed2DFormatCase(const Compressed2DFormatCase &other);
    Compressed2DFormatCase &operator=(const Compressed2DFormatCase &other);

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    std::vector<std::string> m_filenames;

    glu::Texture2D *m_texture;
    TextureRenderer m_renderer;
};

Compressed2DFormatCase::Compressed2DFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                               const glu::ContextInfo &renderCtxInfo, const char *name,
                                               const char *description, const std::vector<std::string> &filenames)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(renderCtxInfo)
    , m_filenames(filenames)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_100_ES, glu::PRECISION_MEDIUMP)
{
}

Compressed2DFormatCase::~Compressed2DFormatCase(void)
{
    deinit();
}

void Compressed2DFormatCase::init(void)
{
    // Create texture.
    m_texture =
        Texture2D::create(m_renderCtx, m_renderCtxInfo, m_testCtx.getArchive(), (int)m_filenames.size(), m_filenames);
}

void Compressed2DFormatCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

Compressed2DFormatCase::IterateResult Compressed2DFormatCase::iterate(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    TestLog &log             = m_testCtx.getLog();
    RandomViewport viewport(m_renderCtx.getRenderTarget(), m_texture->getRefTexture().getWidth(),
                            m_texture->getRefTexture().getHeight(), deStringHash(getName()));
    tcu::Surface renderedFrame(viewport.width, viewport.height);
    tcu::Surface referenceFrame(viewport.width, viewport.height);
    tcu::RGBA threshold = m_renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() + tcu::RGBA(1, 1, 1, 1);
    vector<float> texCoord;

    computeQuadTexCoord2D(texCoord, tcu::Vec2(0.0f, 0.0f), tcu::Vec2(1.0f, 1.0f));

    // Setup base viewport.
    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

    // Bind to unit 0.
    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_2D, m_texture->getGLTexture());

    // Setup nearest neighbor filtering and clamp-to-edge.
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Set texturing state");

    // Draw.
    m_renderer.renderQuad(0, &texCoord[0], TEXTURETYPE_2D);
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels()");

    // Compute reference.
    ReferenceParams refParams(TEXTURETYPE_2D);
    refParams.sampler = Sampler(Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE,
                                Sampler::NEAREST, Sampler::NEAREST);
    sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat()),
                  m_texture->getRefTexture(), &texCoord[0], refParams);

    // Compare and log.
    bool isOk = compareImages(log, referenceFrame, renderedFrame, threshold);

    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                            isOk ? "Pass" : "Image comparison failed");

    return STOP;
}

// CompressedCubeFormatCase

class CompressedCubeFormatCase : public tcu::TestCase
{
public:
    CompressedCubeFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                             const glu::ContextInfo &renderCtxInfo, const char *name, const char *description,
                             const std::vector<std::string> &filenames);
    ~CompressedCubeFormatCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    CompressedCubeFormatCase(const CompressedCubeFormatCase &other);
    CompressedCubeFormatCase &operator=(const CompressedCubeFormatCase &other);

    bool testFace(tcu::CubeFace face);

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    std::vector<std::string> m_filenames;

    glu::TextureCube *m_texture;
    TextureRenderer m_renderer;

    int m_curFace;
    bool m_isOk;
};

CompressedCubeFormatCase::CompressedCubeFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                   const glu::ContextInfo &renderCtxInfo, const char *name,
                                                   const char *description, const std::vector<std::string> &filenames)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(renderCtxInfo)
    , m_filenames(filenames)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_100_ES, glu::PRECISION_MEDIUMP)
    , m_curFace(0)
    , m_isOk(false)
{
}

CompressedCubeFormatCase::~CompressedCubeFormatCase(void)
{
    deinit();
}

void CompressedCubeFormatCase::init(void)
{
    // Create texture.
    DE_ASSERT(m_filenames.size() % 6 == 0);
    m_texture = TextureCube::create(m_renderCtx, m_renderCtxInfo, m_testCtx.getArchive(), (int)m_filenames.size() / 6,
                                    m_filenames);

    m_curFace = 0;
    m_isOk    = true;
}

void CompressedCubeFormatCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

bool CompressedCubeFormatCase::testFace(tcu::CubeFace face)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    TestLog &log             = m_testCtx.getLog();
    RandomViewport viewport(m_renderCtx.getRenderTarget(), m_texture->getRefTexture().getSize(),
                            m_texture->getRefTexture().getSize(), deStringHash(getName()) + (uint32_t)face);
    tcu::Surface renderedFrame(viewport.width, viewport.height);
    tcu::Surface referenceFrame(viewport.width, viewport.height);
    Sampler sampler(Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::NEAREST,
                    Sampler::NEAREST);
    tcu::RGBA threshold = m_renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() + tcu::RGBA(1, 1, 1, 1);
    vector<float> texCoord;

    computeQuadTexCoordCube(texCoord, face);

    // \todo [2011-10-28 pyry] Image set name / section?
    log << TestLog::Message << face << TestLog::EndMessage;

    // Setup base viewport.
    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

    // Bind to unit 0.
    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_CUBE_MAP, m_texture->getGLTexture());

    // Setup nearest neighbor filtering and clamp-to-edge.
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Set texturing state");

    m_renderer.renderQuad(0, &texCoord[0], TEXTURETYPE_CUBE);
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels()");

    // Compute reference.
    sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat()),
                  m_texture->getRefTexture(), &texCoord[0], ReferenceParams(TEXTURETYPE_CUBE, sampler));

    // Compare and log.
    return compareImages(log, referenceFrame, renderedFrame, threshold);
}

CompressedCubeFormatCase::IterateResult CompressedCubeFormatCase::iterate(void)
{
    // Execute test for all faces.
    if (!testFace((tcu::CubeFace)m_curFace))
        m_isOk = false;

    m_curFace += 1;

    if (m_curFace == tcu::CUBEFACE_LAST)
    {
        m_testCtx.setTestResult(m_isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                m_isOk ? "Pass" : "Image comparison failed");
        return STOP;
    }
    else
        return CONTINUE;
}

vector<string> toStringVector(const char *const *str, int numStr)
{
    vector<string> v;
    v.resize(numStr);
    for (int i = 0; i < numStr; i++)
        v[i] = str[i];
    return v;
}

void TextureFormatTests::init(void)
{
    struct
    {
        const char *name;
        uint32_t format;
        uint32_t dataType;
    } texFormats[] = {{"a8", GL_ALPHA, GL_UNSIGNED_BYTE},
                      {"l8", GL_LUMINANCE, GL_UNSIGNED_BYTE},
                      {"la88", GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE},
                      {"rgb565", GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
                      {"rgb888", GL_RGB, GL_UNSIGNED_BYTE},
                      {"rgba4444", GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},
                      {"rgba5551", GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1},
                      {"rgba8888", GL_RGBA, GL_UNSIGNED_BYTE}};

    for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(texFormats); formatNdx++)
    {
        uint32_t format        = texFormats[formatNdx].format;
        uint32_t dataType      = texFormats[formatNdx].dataType;
        string nameBase        = texFormats[formatNdx].name;
        string descriptionBase = string(glu::getTextureFormatName(format)) + ", " + glu::getTypeName(dataType);

        addChild(new Texture2DFormatCase(m_testCtx, m_context.getRenderContext(), (nameBase + "_2d_pot").c_str(),
                                         (descriptionBase + ", GL_TEXTURE_2D").c_str(), format, dataType, 128, 128));
        addChild(new Texture2DFormatCase(m_testCtx, m_context.getRenderContext(), (nameBase + "_2d_npot").c_str(),
                                         (descriptionBase + ", GL_TEXTURE_2D").c_str(), format, dataType, 63, 112));
        addChild(new TextureCubeFormatCase(m_testCtx, m_context.getRenderContext(), (nameBase + "_cube_pot").c_str(),
                                           (descriptionBase + ", GL_TEXTURE_CUBE_MAP").c_str(), format, dataType, 64,
                                           64));
        addChild(new TextureCubeFormatCase(m_testCtx, m_context.getRenderContext(), (nameBase + "_cube_npot").c_str(),
                                           (descriptionBase + ", GL_TEXTURE_CUBE_MAP").c_str(), format, dataType, 57,
                                           57));
    }

    // ETC-1 compressed formats.
    {
        static const char *filenames[] = {"data/etc1/photo_helsinki_mip_0.pkm", "data/etc1/photo_helsinki_mip_1.pkm",
                                          "data/etc1/photo_helsinki_mip_2.pkm", "data/etc1/photo_helsinki_mip_3.pkm",
                                          "data/etc1/photo_helsinki_mip_4.pkm", "data/etc1/photo_helsinki_mip_5.pkm",
                                          "data/etc1/photo_helsinki_mip_6.pkm", "data/etc1/photo_helsinki_mip_7.pkm"};
        addChild(new Compressed2DFormatCase(m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(),
                                            "etc1_2d_pot", "GL_ETC1_RGB8_OES, GL_TEXTURE_2D",
                                            toStringVector(filenames, DE_LENGTH_OF_ARRAY(filenames))));
    }

    {
        vector<string> filenames;
        filenames.push_back("data/etc1/photo_helsinki_113x89.pkm");
        addChild(new Compressed2DFormatCase(m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(),
                                            "etc1_2d_npot", "GL_ETC1_RGB8_OES, GL_TEXTURE_2D", filenames));
    }

    {
        static const char *faceExt[] = {"neg_x", "pos_x", "neg_y", "pos_y", "neg_z", "pos_z"};

        const int potNumLevels = 7;
        vector<string> potFilenames;
        for (int level = 0; level < potNumLevels; level++)
            for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
                potFilenames.push_back(string("data/etc1/skybox_") + faceExt[face] + "_mip_" + de::toString(level) +
                                       ".pkm");

        addChild(new CompressedCubeFormatCase(m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(),
                                              "etc1_cube_pot", "GL_ETC1_RGB8_OES, GL_TEXTURE_CUBE_MAP", potFilenames));

        vector<string> npotFilenames;
        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
            npotFilenames.push_back(string("data/etc1/skybox_61x61_") + faceExt[face] + ".pkm");

        addChild(new CompressedCubeFormatCase(m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(),
                                              "etc1_cube_npot", "GL_ETC_RGB8_OES, GL_TEXTURE_CUBE_MAP", npotFilenames));
    }
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
