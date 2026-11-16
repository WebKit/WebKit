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

#include "es3fTextureFormatTests.hpp"
#include "gluPixelTransfer.hpp"
#include "gluStrUtil.hpp"
#include "gluTexture.hpp"
#include "gluTextureUtil.hpp"
#include "glsTextureTestUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "gluContextInfo.hpp"
#include "deUniquePtr.hpp"

using std::string;
using std::vector;
using tcu::TestLog;

using de::MovePtr;
using glu::ContextInfo;

namespace deqp
{
namespace gles3
{
namespace Functional
{

using namespace deqp::gls;
using namespace deqp::gls::TextureTestUtil;
using namespace glu::TextureTestUtil;
using tcu::Sampler;

namespace
{

void checkSupport(const glu::ContextInfo &info, uint32_t internalFormat)
{
    if (internalFormat == GL_SR8_EXT && !info.isExtensionSupported("GL_EXT_texture_sRGB_R8"))
        TCU_THROW(NotSupportedError, "GL_EXT_texture_sRGB_R8 is not supported.");

    if (internalFormat == GL_SRG8_EXT && !info.isExtensionSupported("GL_EXT_texture_sRGB_RG8"))
        TCU_THROW(NotSupportedError, "GL_EXT_texture_sRGB_RG8 is not supported.");
}

} // namespace

// Texture2DFormatCase

class Texture2DFormatCase : public tcu::TestCase
{
public:
    Texture2DFormatCase(tcu::TestContext &testCtx, Context &context, const char *name, const char *description,
                        uint32_t format, uint32_t dataType, int width, int height);
    Texture2DFormatCase(tcu::TestContext &testCtx, Context &context, const char *name, const char *description,
                        uint32_t internalFormat, int width, int height);
    ~Texture2DFormatCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture2DFormatCase(const Texture2DFormatCase &other);
    Texture2DFormatCase &operator=(const Texture2DFormatCase &other);

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    uint32_t m_format;
    uint32_t m_dataType;
    int m_width;
    int m_height;

    glu::Texture2D *m_texture;
    TextureRenderer m_renderer;
};

Texture2DFormatCase::Texture2DFormatCase(tcu::TestContext &testCtx, Context &context, const char *name,
                                         const char *description, uint32_t format, uint32_t dataType, int width,
                                         int height)
    : TestCase(testCtx, name, description)
    , m_renderCtx(context.getRenderContext())
    , m_renderCtxInfo(context.getContextInfo())
    , m_format(format)
    , m_dataType(dataType)
    , m_width(width)
    , m_height(height)
    , m_texture(nullptr)
    , m_renderer(context.getRenderContext(), testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
{
}

Texture2DFormatCase::Texture2DFormatCase(tcu::TestContext &testCtx, Context &context, const char *name,
                                         const char *description, uint32_t internalFormat, int width, int height)
    : TestCase(testCtx, name, description)
    , m_renderCtx(context.getRenderContext())
    , m_renderCtxInfo(context.getContextInfo())
    , m_format(internalFormat)
    , m_dataType(GL_NONE)
    , m_width(width)
    , m_height(height)
    , m_texture(nullptr)
    , m_renderer(context.getRenderContext(), testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
{
}

Texture2DFormatCase::~Texture2DFormatCase(void)
{
    deinit();
}

void Texture2DFormatCase::init(void)
{
    checkSupport(m_renderCtxInfo, m_format);

    TestLog &log = m_testCtx.getLog();
    tcu::TextureFormat fmt =
        m_dataType ? glu::mapGLTransferFormat(m_format, m_dataType) : glu::mapGLInternalFormat(m_format);
    tcu::TextureFormatInfo spec = tcu::getTextureFormatInfo(fmt);
    std::ostringstream fmtName;

    if (m_dataType)
        fmtName << glu::getTextureFormatStr(m_format) << ", " << glu::getTypeStr(m_dataType);
    else
        fmtName << glu::getTextureFormatStr(m_format);

    log << TestLog::Message << "2D texture, " << fmtName.str() << ", " << m_width << "x" << m_height
        << ",\n  fill with " << formatGradient(&spec.valueMin, &spec.valueMax) << " gradient" << TestLog::EndMessage;

    m_texture =
        m_dataType != GL_NONE ?
            new glu::Texture2D(m_renderCtx, m_format, m_dataType, m_width, m_height) // Implicit internal format.
            :
            new glu::Texture2D(m_renderCtx, m_format, m_width, m_height); // Explicit internal format.

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
        << "\n  WRAP_S = " << glu::getTextureParameterValueStr(GL_TEXTURE_WRAP_S, wrapS)
        << "\n  WRAP_T = " << glu::getTextureParameterValueStr(GL_TEXTURE_WRAP_T, wrapT)
        << "\n  MIN_FILTER = " << glu::getTextureParameterValueStr(GL_TEXTURE_MIN_FILTER, minFilter)
        << "\n  MAG_FILTER = " << glu::getTextureParameterValueStr(GL_TEXTURE_MAG_FILTER, magFilter)
        << TestLog::EndMessage;

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
    TextureCubeFormatCase(tcu::TestContext &testCtx, Context &context, const char *name, const char *description,
                          uint32_t format, uint32_t dataType, int width, int height);
    TextureCubeFormatCase(tcu::TestContext &testCtx, Context &context, const char *name, const char *description,
                          uint32_t internalFormat, int width, int height);
    ~TextureCubeFormatCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    TextureCubeFormatCase(const TextureCubeFormatCase &other);
    TextureCubeFormatCase &operator=(const TextureCubeFormatCase &other);

    bool testFace(tcu::CubeFace face);

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    uint32_t m_format;
    uint32_t m_dataType;
    int m_width;
    int m_height;

    glu::TextureCube *m_texture;
    TextureRenderer m_renderer;

    int m_curFace;
    bool m_isOk;
};

TextureCubeFormatCase::TextureCubeFormatCase(tcu::TestContext &testCtx, Context &context, const char *name,
                                             const char *description, uint32_t format, uint32_t dataType, int width,
                                             int height)
    : TestCase(testCtx, name, description)
    , m_renderCtx(context.getRenderContext())
    , m_renderCtxInfo(context.getContextInfo())
    , m_format(format)
    , m_dataType(dataType)
    , m_width(width)
    , m_height(height)
    , m_texture(nullptr)
    , m_renderer(context.getRenderContext(), testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
    , m_curFace(0)
    , m_isOk(false)
{
}

TextureCubeFormatCase::TextureCubeFormatCase(tcu::TestContext &testCtx, Context &context, const char *name,
                                             const char *description, uint32_t internalFormat, int width, int height)
    : TestCase(testCtx, name, description)
    , m_renderCtx(context.getRenderContext())
    , m_renderCtxInfo(context.getContextInfo())
    , m_format(internalFormat)
    , m_dataType(GL_NONE)
    , m_width(width)
    , m_height(height)
    , m_texture(nullptr)
    , m_renderer(context.getRenderContext(), testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
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
    checkSupport(m_renderCtxInfo, m_format);

    TestLog &log = m_testCtx.getLog();
    tcu::TextureFormat fmt =
        m_dataType ? glu::mapGLTransferFormat(m_format, m_dataType) : glu::mapGLInternalFormat(m_format);
    tcu::TextureFormatInfo spec = tcu::getTextureFormatInfo(fmt);
    std::ostringstream fmtName;

    if (m_dataType)
        fmtName << glu::getTextureFormatStr(m_format) << ", " << glu::getTypeStr(m_dataType);
    else
        fmtName << glu::getTextureFormatStr(m_format);

    log << TestLog::Message << "Cube map texture, " << fmtName.str() << ", " << m_width << "x" << m_height
        << ",\n  fill with " << formatGradient(&spec.valueMin, &spec.valueMax) << " gradient" << TestLog::EndMessage;

    DE_ASSERT(m_width == m_height);
    m_texture = m_dataType != GL_NONE ?
                    new glu::TextureCube(m_renderCtx, m_format, m_dataType, m_width) // Implicit internal format.
                    :
                    new glu::TextureCube(m_renderCtx, m_format, m_width); // Explicit internal format.

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
    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_renderCtx.getFunctions();
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
    renderParams.sampler.seamlessCubeMap = true;
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

// Texture2DArrayFormatCase

class Texture2DArrayFormatCase : public tcu::TestCase
{
public:
    Texture2DArrayFormatCase(tcu::TestContext &testCtx, Context &context, const char *name, const char *description,
                             uint32_t format, uint32_t dataType, int width, int height, int numLayers);
    Texture2DArrayFormatCase(tcu::TestContext &testCtx, Context &context, const char *name, const char *description,
                             uint32_t internalFormat, int width, int height, int numLayers);
    ~Texture2DArrayFormatCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture2DArrayFormatCase(const Texture2DArrayFormatCase &other);
    Texture2DArrayFormatCase &operator=(const Texture2DArrayFormatCase &other);

    bool testLayer(int layerNdx);

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    uint32_t m_format;
    uint32_t m_dataType;
    int m_width;
    int m_height;
    int m_numLayers;

    glu::Texture2DArray *m_texture;
    TextureTestUtil::TextureRenderer m_renderer;

    int m_curLayer;
};

Texture2DArrayFormatCase::Texture2DArrayFormatCase(tcu::TestContext &testCtx, Context &context, const char *name,
                                                   const char *description, uint32_t format, uint32_t dataType,
                                                   int width, int height, int numLayers)
    : TestCase(testCtx, name, description)
    , m_renderCtx(context.getRenderContext())
    , m_renderCtxInfo(context.getContextInfo())
    , m_format(format)
    , m_dataType(dataType)
    , m_width(width)
    , m_height(height)
    , m_numLayers(numLayers)
    , m_texture(nullptr)
    , m_renderer(context.getRenderContext(), testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
    , m_curLayer(0)
{
}

Texture2DArrayFormatCase::Texture2DArrayFormatCase(tcu::TestContext &testCtx, Context &context, const char *name,
                                                   const char *description, uint32_t internalFormat, int width,
                                                   int height, int numLayers)
    : TestCase(testCtx, name, description)
    , m_renderCtx(context.getRenderContext())
    , m_renderCtxInfo(context.getContextInfo())
    , m_format(internalFormat)
    , m_dataType(GL_NONE)
    , m_width(width)
    , m_height(height)
    , m_numLayers(numLayers)
    , m_texture(nullptr)
    , m_renderer(context.getRenderContext(), testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
    , m_curLayer(0)
{
}

Texture2DArrayFormatCase::~Texture2DArrayFormatCase(void)
{
    deinit();
}

void Texture2DArrayFormatCase::init(void)
{
    checkSupport(m_renderCtxInfo, m_format);

    m_texture = m_dataType != GL_NONE ? new glu::Texture2DArray(m_renderCtx, m_format, m_dataType, m_width, m_height,
                                                                m_numLayers) // Implicit internal format.
                                        :
                                        new glu::Texture2DArray(m_renderCtx, m_format, m_width, m_height,
                                                                m_numLayers); // Explicit internal format.

    tcu::TextureFormatInfo spec = tcu::getTextureFormatInfo(m_texture->getRefTexture().getFormat());

    // Fill level 0.
    m_texture->getRefTexture().allocLevel(0);
    tcu::fillWithComponentGradients(m_texture->getRefTexture().getLevel(0), spec.valueMin, spec.valueMax);

    // Initialize state.
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    m_curLayer = 0;
}

void Texture2DArrayFormatCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

bool Texture2DArrayFormatCase::testLayer(int layerNdx)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    TestLog &log             = m_testCtx.getLog();
    RandomViewport viewport(m_renderCtx.getRenderTarget(), m_width, m_height, deStringHash(getName()));
    tcu::Surface renderedFrame(viewport.width, viewport.height);
    tcu::Surface referenceFrame(viewport.width, viewport.height);
    tcu::RGBA threshold = m_renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() + tcu::RGBA(1, 1, 1, 1);
    vector<float> texCoord;
    ReferenceParams renderParams(TEXTURETYPE_2D_ARRAY);
    tcu::TextureFormatInfo spec = tcu::getTextureFormatInfo(m_texture->getRefTexture().getFormat());

    renderParams.samplerType = getSamplerType(m_texture->getRefTexture().getFormat());
    renderParams.sampler     = Sampler(Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE,
                                       Sampler::NEAREST, Sampler::NEAREST);
    renderParams.colorScale  = spec.lookupScale;
    renderParams.colorBias   = spec.lookupBias;

    computeQuadTexCoord2DArray(texCoord, layerNdx, tcu::Vec2(0.0f, 0.0f), tcu::Vec2(1.0f, 1.0f));

    // Setup base viewport.
    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

    // Upload texture data to GL.
    m_texture->upload();

    // Bind to unit 0.
    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_2D_ARRAY, m_texture->getGLTexture());

    // Setup nearest neighbor filtering and clamp-to-edge.
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Set texturing state");

    // Draw.
    m_renderer.renderQuad(0, &texCoord[0], renderParams);
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());

    // Compute reference.
    sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat()),
                  m_texture->getRefTexture(), &texCoord[0], renderParams);

    // Compare and log.
    return compareImages(log, (string("Layer" + de::toString(layerNdx))).c_str(),
                         (string("Layer " + de::toString(layerNdx))).c_str(), referenceFrame, renderedFrame, threshold);
}

Texture2DArrayFormatCase::IterateResult Texture2DArrayFormatCase::iterate(void)
{
    // Execute test for all layers.
    bool isOk = testLayer(m_curLayer);

    if (!isOk && m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");

    m_curLayer += 1;

    return m_curLayer < m_texture->getRefTexture().getNumLayers() ? CONTINUE : STOP;
}

// Texture2DFormatCase

class Texture3DFormatCase : public tcu::TestCase
{
public:
    Texture3DFormatCase(tcu::TestContext &testCtx, Context &context, const char *name, const char *description,
                        uint32_t format, uint32_t dataType, int width, int height, int depth);
    Texture3DFormatCase(tcu::TestContext &testCtx, Context &context, const char *name, const char *description,
                        uint32_t internalFormat, int width, int height, int depth);
    ~Texture3DFormatCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture3DFormatCase(const Texture3DFormatCase &other);
    Texture3DFormatCase &operator=(const Texture3DFormatCase &other);

    bool testSlice(int sliceNdx);

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    uint32_t m_format;
    uint32_t m_dataType;
    int m_width;
    int m_height;
    int m_depth;

    glu::Texture3D *m_texture;
    TextureTestUtil::TextureRenderer m_renderer;

    int m_curSlice;
};

Texture3DFormatCase::Texture3DFormatCase(tcu::TestContext &testCtx, Context &context, const char *name,
                                         const char *description, uint32_t format, uint32_t dataType, int width,
                                         int height, int depth)
    : TestCase(testCtx, name, description)
    , m_renderCtx(context.getRenderContext())
    , m_renderCtxInfo(context.getContextInfo())
    , m_format(format)
    , m_dataType(dataType)
    , m_width(width)
    , m_height(height)
    , m_depth(depth)
    , m_texture(nullptr)
    , m_renderer(context.getRenderContext(), testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
    , m_curSlice(0)
{
}

Texture3DFormatCase::Texture3DFormatCase(tcu::TestContext &testCtx, Context &context, const char *name,
                                         const char *description, uint32_t internalFormat, int width, int height,
                                         int depth)
    : TestCase(testCtx, name, description)
    , m_renderCtx(context.getRenderContext())
    , m_renderCtxInfo(context.getContextInfo())
    , m_format(internalFormat)
    , m_dataType(GL_NONE)
    , m_width(width)
    , m_height(height)
    , m_depth(depth)
    , m_texture(nullptr)
    , m_renderer(context.getRenderContext(), testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
    , m_curSlice(0)
{
}

Texture3DFormatCase::~Texture3DFormatCase(void)
{
    deinit();
}

void Texture3DFormatCase::init(void)
{
    checkSupport(m_renderCtxInfo, m_format);

    m_texture = m_dataType != GL_NONE ?
                    new glu::Texture3D(m_renderCtx, m_format, m_dataType, m_width, m_height,
                                       m_depth) // Implicit internal format.
                    :
                    new glu::Texture3D(m_renderCtx, m_format, m_width, m_height, m_depth); // Explicit internal format.

    tcu::TextureFormatInfo spec = tcu::getTextureFormatInfo(m_texture->getRefTexture().getFormat());

    // Fill level 0.
    m_texture->getRefTexture().allocLevel(0);
    tcu::fillWithComponentGradients(m_texture->getRefTexture().getLevel(0), spec.valueMin, spec.valueMax);

    // Initialize state.
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    m_curSlice = 0;
}

void Texture3DFormatCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

bool Texture3DFormatCase::testSlice(int sliceNdx)
{
    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_renderCtx.getFunctions();
    RandomViewport viewport(m_renderCtx.getRenderTarget(), m_width, m_height, deStringHash(getName()));
    tcu::Surface renderedFrame(viewport.width, viewport.height);
    tcu::Surface referenceFrame(viewport.width, viewport.height);
    tcu::RGBA threshold = m_renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() + tcu::RGBA(1, 1, 1, 1);
    vector<float> texCoord;
    ReferenceParams renderParams(TEXTURETYPE_3D);
    tcu::TextureFormatInfo spec = tcu::getTextureFormatInfo(m_texture->getRefTexture().getFormat());
    float r                     = ((float)sliceNdx + 0.5f) / (float)m_depth;

    renderParams.samplerType = getSamplerType(m_texture->getRefTexture().getFormat());
    renderParams.sampler     = Sampler(Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE,
                                       Sampler::NEAREST, Sampler::NEAREST);
    renderParams.colorScale  = spec.lookupScale;
    renderParams.colorBias   = spec.lookupBias;

    computeQuadTexCoord3D(texCoord, tcu::Vec3(0.0f, 0.0f, r), tcu::Vec3(1.0f, 1.0f, r), tcu::IVec3(0, 1, 2));

    // Setup base viewport.
    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

    // Upload texture data to GL.
    m_texture->upload();

    // Bind to unit 0.
    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_3D, m_texture->getGLTexture());

    // Setup nearest neighbor filtering and clamp-to-edge.
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Set texturing state");

    // Draw.
    m_renderer.renderQuad(0, &texCoord[0], renderParams);
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());

    // Compute reference.
    sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat()),
                  m_texture->getRefTexture(), &texCoord[0], renderParams);

    // Compare and log.
    return compareImages(log, (string("Slice" + de::toString(sliceNdx))).c_str(),
                         (string("Slice " + de::toString(sliceNdx))).c_str(), referenceFrame, renderedFrame, threshold);
}

Texture3DFormatCase::IterateResult Texture3DFormatCase::iterate(void)
{
    // Execute test for all slices.
    bool isOk = testSlice(m_curSlice);

    if (!isOk && m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");

    m_curSlice += 1;

    return m_curSlice < m_texture->getRefTexture().getDepth() ? CONTINUE : STOP;
}

// Compressed2FormatCase

class Compressed2DFormatCase : public tcu::TestCase
{
public:
    Compressed2DFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                           const glu::ContextInfo &renderCtxInfo, const char *name, const char *description,
                           tcu::CompressedTexFormat format, uint32_t randomSeed, int width, int height);
    ~Compressed2DFormatCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Compressed2DFormatCase(const Compressed2DFormatCase &other);
    Compressed2DFormatCase &operator=(const Compressed2DFormatCase &other);

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    tcu::CompressedTexFormat m_format;

    uint32_t m_randomSeed;
    int m_width;
    int m_height;

    glu::Texture2D *m_texture;
    TextureTestUtil::TextureRenderer m_renderer;
};

Compressed2DFormatCase::Compressed2DFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                               const glu::ContextInfo &renderCtxInfo, const char *name,
                                               const char *description, tcu::CompressedTexFormat format,
                                               uint32_t randomSeed, int width, int height)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(renderCtxInfo)
    , m_format(format)
    , m_randomSeed(randomSeed)
    , m_width(width)
    , m_height(height)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
{
}

Compressed2DFormatCase::~Compressed2DFormatCase(void)
{
    deinit();
}

void Compressed2DFormatCase::init(void)
{
    // Create texture.
    tcu::CompressedTexture compressedTexture(m_format, m_width, m_height);
    int dataSize  = compressedTexture.getDataSize();
    uint8_t *data = (uint8_t *)compressedTexture.getData();
    de::Random rnd(m_randomSeed);

    for (int i = 0; i < dataSize; i++)
        data[i] = rnd.getUint32() & 0xff;

    m_texture = new glu::Texture2D(m_renderCtx, m_renderCtxInfo, 1, &compressedTexture);
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
    ReferenceParams renderParams(TEXTURETYPE_2D);
    tcu::TextureFormatInfo spec = tcu::getTextureFormatInfo(m_texture->getRefTexture().getFormat());

    renderParams.samplerType = getSamplerType(m_texture->getRefTexture().getFormat());
    renderParams.sampler     = Sampler(Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE,
                                       Sampler::NEAREST, Sampler::NEAREST);
    renderParams.colorScale  = spec.lookupScale;
    renderParams.colorBias   = spec.lookupBias;

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
    m_renderer.renderQuad(0, &texCoord[0], renderParams);
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());

    // Compute reference.
    sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat()),
                  m_texture->getRefTexture(), &texCoord[0], renderParams);

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
                             tcu::CompressedTexFormat format, uint32_t randomSeed, int width, int height);

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

    tcu::CompressedTexFormat m_format;

    uint32_t m_randomSeed;
    int m_width;
    int m_height;

    glu::TextureCube *m_texture;
    TextureTestUtil::TextureRenderer m_renderer;

    int m_curFace;
    bool m_isOk;
};

CompressedCubeFormatCase::CompressedCubeFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                   const glu::ContextInfo &renderCtxInfo, const char *name,
                                                   const char *description, tcu::CompressedTexFormat format,
                                                   uint32_t randomSeed, int width, int height)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(renderCtxInfo)
    , m_format(format)
    , m_randomSeed(randomSeed)
    , m_width(width)
    , m_height(height)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
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
    vector<tcu::CompressedTexture> levels(tcu::CUBEFACE_LAST);
    de::Random rnd(m_randomSeed);

    for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
    {
        levels[face].setStorage(m_format, m_width, m_height);

        int dataSize  = levels[face].getDataSize();
        uint8_t *data = (uint8_t *)levels[face].getData();

        for (int i = 0; i < dataSize; i++)
            data[i] = rnd.getUint32() & 0xff;
    }

    m_texture = new glu::TextureCube(m_renderCtx, m_renderCtxInfo, 1, &levels[0]);

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
    tcu::RGBA threshold = m_renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() + tcu::RGBA(1, 1, 1, 1);
    vector<float> texCoord;
    ReferenceParams renderParams(TEXTURETYPE_CUBE);
    tcu::TextureFormatInfo spec = tcu::getTextureFormatInfo(m_texture->getRefTexture().getFormat());

    renderParams.samplerType = getSamplerType(m_texture->getRefTexture().getFormat());
    renderParams.sampler     = Sampler(Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE,
                                       Sampler::NEAREST, Sampler::NEAREST);
    renderParams.sampler.seamlessCubeMap = true;
    renderParams.colorScale              = spec.lookupScale;
    renderParams.colorBias               = spec.lookupBias;

    computeQuadTexCoordCube(texCoord, face);

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

    // Compute reference.
    sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat()),
                  m_texture->getRefTexture(), &texCoord[0], renderParams);

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

// Texture2DFileCase

class Texture2DFileCase : public tcu::TestCase
{
public:
    Texture2DFileCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &renderCtxInfo,
                      const char *name, const char *description, const std::vector<std::string> &filenames);
    ~Texture2DFileCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture2DFileCase(const Texture2DFileCase &other);
    Texture2DFileCase &operator=(const Texture2DFileCase &other);

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    std::vector<std::string> m_filenames;

    glu::Texture2D *m_texture;
    TextureRenderer m_renderer;
};

Texture2DFileCase::Texture2DFileCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                     const glu::ContextInfo &renderCtxInfo, const char *name, const char *description,
                                     const std::vector<std::string> &filenames)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(renderCtxInfo)
    , m_filenames(filenames)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
{
}

Texture2DFileCase::~Texture2DFileCase(void)
{
    deinit();
}

void Texture2DFileCase::init(void)
{
    // Create texture.
    m_texture = glu::Texture2D::create(m_renderCtx, m_renderCtxInfo, m_testCtx.getArchive(), (int)m_filenames.size(),
                                       m_filenames);
}

void Texture2DFileCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

Texture2DFileCase::IterateResult Texture2DFileCase::iterate(void)
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

// TextureCubeFileCase

class TextureCubeFileCase : public tcu::TestCase
{
public:
    TextureCubeFileCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &renderCtxInfo,
                        const char *name, const char *description, const std::vector<std::string> &filenames);
    ~TextureCubeFileCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    TextureCubeFileCase(const TextureCubeFileCase &other);
    TextureCubeFileCase &operator=(const TextureCubeFileCase &other);

    bool testFace(tcu::CubeFace face);

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    std::vector<std::string> m_filenames;

    glu::TextureCube *m_texture;
    TextureRenderer m_renderer;

    int m_curFace;
    bool m_isOk;
};

TextureCubeFileCase::TextureCubeFileCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                         const glu::ContextInfo &renderCtxInfo, const char *name,
                                         const char *description, const std::vector<std::string> &filenames)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(renderCtxInfo)
    , m_filenames(filenames)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
    , m_curFace(0)
    , m_isOk(false)
{
}

TextureCubeFileCase::~TextureCubeFileCase(void)
{
    deinit();
}

void TextureCubeFileCase::init(void)
{
    // Create texture.
    DE_ASSERT(m_filenames.size() % 6 == 0);
    m_texture = glu::TextureCube::create(m_renderCtx, m_renderCtxInfo, m_testCtx.getArchive(),
                                         (int)m_filenames.size() / 6, m_filenames);

    m_curFace = 0;
    m_isOk    = true;
}

void TextureCubeFileCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

bool TextureCubeFileCase::testFace(tcu::CubeFace face)
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

TextureCubeFileCase::IterateResult TextureCubeFileCase::iterate(void)
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

// TextureFormatTests

TextureFormatTests::TextureFormatTests(Context &context) : TestCaseGroup(context, "format", "Texture Format Tests")
{
}

TextureFormatTests::~TextureFormatTests(void)
{
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
    tcu::TestCaseGroup *unsizedGroup    = nullptr;
    tcu::TestCaseGroup *sizedGroup      = nullptr;
    tcu::TestCaseGroup *compressedGroup = nullptr;
    addChild((unsizedGroup = new tcu::TestCaseGroup(m_testCtx, "unsized", "Unsized formats")));
    addChild((sizedGroup = new tcu::TestCaseGroup(m_testCtx, "sized", "Sized formats")));
    addChild((compressedGroup = new tcu::TestCaseGroup(m_testCtx, "compressed", "Compressed formats")));

    tcu::TestCaseGroup *sized2DGroup      = nullptr;
    tcu::TestCaseGroup *sizedCubeGroup    = nullptr;
    tcu::TestCaseGroup *sized2DArrayGroup = nullptr;
    tcu::TestCaseGroup *sized3DGroup      = nullptr;
    sizedGroup->addChild((sized2DGroup = new tcu::TestCaseGroup(m_testCtx, "2d", "Sized formats (2D)")));
    sizedGroup->addChild((sizedCubeGroup = new tcu::TestCaseGroup(m_testCtx, "cube", "Sized formats (Cubemap)")));
    sizedGroup->addChild(
        (sized2DArrayGroup = new tcu::TestCaseGroup(m_testCtx, "2d_array", "Sized formats (2D Array)")));
    sizedGroup->addChild((sized3DGroup = new tcu::TestCaseGroup(m_testCtx, "3d", "Sized formats (3D)")));

    struct
    {
        const char *name;
        uint32_t format;
        uint32_t dataType;
    } texFormats[] = {{"alpha", GL_ALPHA, GL_UNSIGNED_BYTE},
                      {"luminance", GL_LUMINANCE, GL_UNSIGNED_BYTE},
                      {"luminance_alpha", GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE},
                      {"rgb_unsigned_short_5_6_5", GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
                      {"rgb_unsigned_byte", GL_RGB, GL_UNSIGNED_BYTE},
                      {"rgba_unsigned_short_4_4_4_4", GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},
                      {"rgba_unsigned_short_5_5_5_1", GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1},
                      {"rgba_unsigned_byte", GL_RGBA, GL_UNSIGNED_BYTE}};

    for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(texFormats); formatNdx++)
    {
        uint32_t format        = texFormats[formatNdx].format;
        uint32_t dataType      = texFormats[formatNdx].dataType;
        string nameBase        = texFormats[formatNdx].name;
        string descriptionBase = string(glu::getTextureFormatName(format)) + ", " + glu::getTypeName(dataType);

        unsizedGroup->addChild(new Texture2DFormatCase(m_testCtx, m_context, (nameBase + "_2d_pot").c_str(),
                                                       (descriptionBase + ", GL_TEXTURE_2D").c_str(), format, dataType,
                                                       128, 128));
        unsizedGroup->addChild(new Texture2DFormatCase(m_testCtx, m_context, (nameBase + "_2d_npot").c_str(),
                                                       (descriptionBase + ", GL_TEXTURE_2D").c_str(), format, dataType,
                                                       63, 112));
        unsizedGroup->addChild(new TextureCubeFormatCase(m_testCtx, m_context, (nameBase + "_cube_pot").c_str(),
                                                         (descriptionBase + ", GL_TEXTURE_CUBE_MAP").c_str(), format,
                                                         dataType, 64, 64));
        unsizedGroup->addChild(new TextureCubeFormatCase(m_testCtx, m_context, (nameBase + "_cube_npot").c_str(),
                                                         (descriptionBase + ", GL_TEXTURE_CUBE_MAP").c_str(), format,
                                                         dataType, 57, 57));
        unsizedGroup->addChild(new Texture2DArrayFormatCase(m_testCtx, m_context, (nameBase + "_2d_array_pot").c_str(),
                                                            (descriptionBase + ", GL_TEXTURE_2D_ARRAY").c_str(), format,
                                                            dataType, 64, 64, 8));
        unsizedGroup->addChild(new Texture2DArrayFormatCase(m_testCtx, m_context, (nameBase + "_2d_array_npot").c_str(),
                                                            (descriptionBase + ", GL_TEXTURE_2D_ARRAY").c_str(), format,
                                                            dataType, 63, 57, 7));
        unsizedGroup->addChild(new Texture3DFormatCase(m_testCtx, m_context, (nameBase + "_3d_pot").c_str(),
                                                       (descriptionBase + ", GL_TEXTURE_3D").c_str(), format, dataType,
                                                       8, 32, 16));
        unsizedGroup->addChild(new Texture3DFormatCase(m_testCtx, m_context, (nameBase + "_3d_npot").c_str(),
                                                       (descriptionBase + ", GL_TEXTURE_3D").c_str(), format, dataType,
                                                       11, 31, 7));
    }

    struct
    {
        const char *name;
        uint32_t internalFormat;
    } sizedColorFormats[] = {{
                                 "rgba32f",
                                 GL_RGBA32F,
                             },
                             {
                                 "rgba32i",
                                 GL_RGBA32I,
                             },
                             {
                                 "rgba32ui",
                                 GL_RGBA32UI,
                             },
                             {
                                 "rgba16f",
                                 GL_RGBA16F,
                             },
                             {
                                 "rgba16i",
                                 GL_RGBA16I,
                             },
                             {
                                 "rgba16ui",
                                 GL_RGBA16UI,
                             },
                             {
                                 "rgba8",
                                 GL_RGBA8,
                             },
                             {
                                 "rgba8i",
                                 GL_RGBA8I,
                             },
                             {
                                 "rgba8ui",
                                 GL_RGBA8UI,
                             },
                             {
                                 "srgb8_alpha8",
                                 GL_SRGB8_ALPHA8,
                             },
                             {
                                 "srgb_r8",
                                 GL_SR8_EXT,
                             },
                             {
                                 "srgb_rg8",
                                 GL_SRG8_EXT,
                             },
                             {
                                 "rgb10_a2",
                                 GL_RGB10_A2,
                             },
                             {
                                 "rgb10_a2ui",
                                 GL_RGB10_A2UI,
                             },
                             {
                                 "rgba4",
                                 GL_RGBA4,
                             },
                             {
                                 "rgb5_a1",
                                 GL_RGB5_A1,
                             },
                             {
                                 "rgba8_snorm",
                                 GL_RGBA8_SNORM,
                             },
                             {
                                 "rgb8",
                                 GL_RGB8,
                             },
                             {
                                 "rgb565",
                                 GL_RGB565,
                             },
                             {
                                 "r11f_g11f_b10f",
                                 GL_R11F_G11F_B10F,
                             },
                             {
                                 "rgb32f",
                                 GL_RGB32F,
                             },
                             {
                                 "rgb32i",
                                 GL_RGB32I,
                             },
                             {
                                 "rgb32ui",
                                 GL_RGB32UI,
                             },
                             {
                                 "rgb16f",
                                 GL_RGB16F,
                             },
                             {
                                 "rgb16i",
                                 GL_RGB16I,
                             },
                             {
                                 "rgb16ui",
                                 GL_RGB16UI,
                             },
                             {
                                 "rgb8_snorm",
                                 GL_RGB8_SNORM,
                             },
                             {
                                 "rgb8i",
                                 GL_RGB8I,
                             },
                             {
                                 "rgb8ui",
                                 GL_RGB8UI,
                             },
                             {
                                 "srgb8",
                                 GL_SRGB8,
                             },
                             {
                                 "rgb9_e5",
                                 GL_RGB9_E5,
                             },
                             {
                                 "rg32f",
                                 GL_RG32F,
                             },
                             {
                                 "rg32i",
                                 GL_RG32I,
                             },
                             {
                                 "rg32ui",
                                 GL_RG32UI,
                             },
                             {
                                 "rg16f",
                                 GL_RG16F,
                             },
                             {
                                 "rg16i",
                                 GL_RG16I,
                             },
                             {
                                 "rg16ui",
                                 GL_RG16UI,
                             },
                             {
                                 "rg8",
                                 GL_RG8,
                             },
                             {
                                 "rg8i",
                                 GL_RG8I,
                             },
                             {
                                 "rg8ui",
                                 GL_RG8UI,
                             },
                             {
                                 "rg8_snorm",
                                 GL_RG8_SNORM,
                             },
                             {
                                 "r32f",
                                 GL_R32F,
                             },
                             {
                                 "r32i",
                                 GL_R32I,
                             },
                             {
                                 "r32ui",
                                 GL_R32UI,
                             },
                             {
                                 "r16f",
                                 GL_R16F,
                             },
                             {
                                 "r16i",
                                 GL_R16I,
                             },
                             {
                                 "r16ui",
                                 GL_R16UI,
                             },
                             {
                                 "r8",
                                 GL_R8,
                             },
                             {
                                 "r8i",
                                 GL_R8I,
                             },
                             {
                                 "r8ui",
                                 GL_R8UI,
                             },
                             {
                                 "r8_snorm",
                                 GL_R8_SNORM,
                             }};

    struct
    {
        const char *name;
        uint32_t internalFormat;
    } sizedDepthStencilFormats[] = {// Depth and stencil formats
                                    {"depth_component32f", GL_DEPTH_COMPONENT32F},
                                    {"depth_component24", GL_DEPTH_COMPONENT24},
                                    {"depth_component16", GL_DEPTH_COMPONENT16},
                                    {"depth32f_stencil8", GL_DEPTH32F_STENCIL8},
                                    {"depth24_stencil8", GL_DEPTH24_STENCIL8}};

    for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(sizedColorFormats); formatNdx++)
    {
        uint32_t internalFormat = sizedColorFormats[formatNdx].internalFormat;
        string nameBase         = sizedColorFormats[formatNdx].name;
        string descriptionBase  = glu::getTextureFormatName(internalFormat);

        sized2DGroup->addChild(new Texture2DFormatCase(m_testCtx, m_context, (nameBase + "_pot").c_str(),
                                                       (descriptionBase + ", GL_TEXTURE_2D").c_str(), internalFormat,
                                                       128, 128));
        sized2DGroup->addChild(new Texture2DFormatCase(m_testCtx, m_context, (nameBase + "_npot").c_str(),
                                                       (descriptionBase + ", GL_TEXTURE_2D").c_str(), internalFormat,
                                                       63, 112));
        sizedCubeGroup->addChild(new TextureCubeFormatCase(m_testCtx, m_context, (nameBase + "_pot").c_str(),
                                                           (descriptionBase + ", GL_TEXTURE_CUBE_MAP").c_str(),
                                                           internalFormat, 64, 64));
        sizedCubeGroup->addChild(new TextureCubeFormatCase(m_testCtx, m_context, (nameBase + "_npot").c_str(),
                                                           (descriptionBase + ", GL_TEXTURE_CUBE_MAP").c_str(),
                                                           internalFormat, 57, 57));
        sized2DArrayGroup->addChild(new Texture2DArrayFormatCase(m_testCtx, m_context, (nameBase + "_pot").c_str(),
                                                                 (descriptionBase + ", GL_TEXTURE_2D_ARRAY").c_str(),
                                                                 internalFormat, 64, 64, 8));
        sized2DArrayGroup->addChild(new Texture2DArrayFormatCase(m_testCtx, m_context, (nameBase + "_npot").c_str(),
                                                                 (descriptionBase + ", GL_TEXTURE_2D_ARRAY").c_str(),
                                                                 internalFormat, 63, 57, 7));
        sized3DGroup->addChild(new Texture3DFormatCase(m_testCtx, m_context, (nameBase + "_pot").c_str(),
                                                       (descriptionBase + ", GL_TEXTURE_3D").c_str(), internalFormat, 8,
                                                       32, 16));
        sized3DGroup->addChild(new Texture3DFormatCase(m_testCtx, m_context, (nameBase + "_npot").c_str(),
                                                       (descriptionBase + ", GL_TEXTURE_3D").c_str(), internalFormat,
                                                       11, 31, 7));
    }

    for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(sizedDepthStencilFormats); formatNdx++)
    {
        uint32_t internalFormat = sizedDepthStencilFormats[formatNdx].internalFormat;
        string nameBase         = sizedDepthStencilFormats[formatNdx].name;
        string descriptionBase  = glu::getTextureFormatName(internalFormat);

        sized2DGroup->addChild(new Texture2DFormatCase(m_testCtx, m_context, (nameBase + "_pot").c_str(),
                                                       (descriptionBase + ", GL_TEXTURE_2D").c_str(), internalFormat,
                                                       128, 128));
        sized2DGroup->addChild(new Texture2DFormatCase(m_testCtx, m_context, (nameBase + "_npot").c_str(),
                                                       (descriptionBase + ", GL_TEXTURE_2D").c_str(), internalFormat,
                                                       63, 112));
        sizedCubeGroup->addChild(new TextureCubeFormatCase(m_testCtx, m_context, (nameBase + "_pot").c_str(),
                                                           (descriptionBase + ", GL_TEXTURE_CUBE_MAP").c_str(),
                                                           internalFormat, 64, 64));
        sizedCubeGroup->addChild(new TextureCubeFormatCase(m_testCtx, m_context, (nameBase + "_npot").c_str(),
                                                           (descriptionBase + ", GL_TEXTURE_CUBE_MAP").c_str(),
                                                           internalFormat, 57, 57));
        sized2DArrayGroup->addChild(new Texture2DArrayFormatCase(m_testCtx, m_context, (nameBase + "_pot").c_str(),
                                                                 (descriptionBase + ", GL_TEXTURE_2D_ARRAY").c_str(),
                                                                 internalFormat, 64, 64, 8));
        sized2DArrayGroup->addChild(new Texture2DArrayFormatCase(m_testCtx, m_context, (nameBase + "_npot").c_str(),
                                                                 (descriptionBase + ", GL_TEXTURE_2D_ARRAY").c_str(),
                                                                 internalFormat, 63, 57, 7));
    }

    // ETC-1 compressed formats.
    {
        static const char *filenames[] = {"data/etc1/photo_helsinki_mip_0.pkm", "data/etc1/photo_helsinki_mip_1.pkm",
                                          "data/etc1/photo_helsinki_mip_2.pkm", "data/etc1/photo_helsinki_mip_3.pkm",
                                          "data/etc1/photo_helsinki_mip_4.pkm", "data/etc1/photo_helsinki_mip_5.pkm",
                                          "data/etc1/photo_helsinki_mip_6.pkm", "data/etc1/photo_helsinki_mip_7.pkm"};
        compressedGroup->addChild(new Texture2DFileCase(
            m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), "etc1_2d_pot",
            "GL_ETC1_RGB8_OES, GL_TEXTURE_2D", toStringVector(filenames, DE_LENGTH_OF_ARRAY(filenames))));
    }

    {
        vector<string> filenames;
        filenames.push_back("data/etc1/photo_helsinki_113x89.pkm");
        compressedGroup->addChild(new Texture2DFileCase(m_testCtx, m_context.getRenderContext(),
                                                        m_context.getContextInfo(), "etc1_2d_npot",
                                                        "GL_ETC1_RGB8_OES, GL_TEXTURE_2D", filenames));
    }

    {
        static const char *faceExt[] = {"neg_x", "pos_x", "neg_y", "pos_y", "neg_z", "pos_z"};

        const int potNumLevels = 7;
        vector<string> potFilenames;
        for (int level = 0; level < potNumLevels; level++)
            for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
                potFilenames.push_back(string("data/etc1/skybox_") + faceExt[face] + "_mip_" + de::toString(level) +
                                       ".pkm");

        compressedGroup->addChild(new TextureCubeFileCase(m_testCtx, m_context.getRenderContext(),
                                                          m_context.getContextInfo(), "etc1_cube_pot",
                                                          "GL_ETC1_RGB8_OES, GL_TEXTURE_CUBE_MAP", potFilenames));

        vector<string> npotFilenames;
        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
            npotFilenames.push_back(string("data/etc1/skybox_61x61_") + faceExt[face] + ".pkm");

        compressedGroup->addChild(new TextureCubeFileCase(m_testCtx, m_context.getRenderContext(),
                                                          m_context.getContextInfo(), "etc1_cube_npot",
                                                          "GL_ETC_RGB8_OES, GL_TEXTURE_CUBE_MAP", npotFilenames));
    }

    // ETC-2 and EAC compressed formats.
    struct
    {
        const char *descriptionBase;
        const char *nameBase;
        tcu::CompressedTexFormat format;
    } etc2Formats[] = {{
                           "GL_COMPRESSED_R11_EAC",
                           "eac_r11",
                           tcu::COMPRESSEDTEXFORMAT_EAC_R11,
                       },
                       {
                           "GL_COMPRESSED_SIGNED_R11_EAC",
                           "eac_signed_r11",
                           tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_R11,
                       },
                       {
                           "GL_COMPRESSED_RG11_EAC",
                           "eac_rg11",
                           tcu::COMPRESSEDTEXFORMAT_EAC_RG11,
                       },
                       {
                           "GL_COMPRESSED_SIGNED_RG11_EAC",
                           "eac_signed_rg11",
                           tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_RG11,
                       },
                       {
                           "GL_COMPRESSED_RGB8_ETC2",
                           "etc2_rgb8",
                           tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8,
                       },
                       {
                           "GL_COMPRESSED_SRGB8_ETC2",
                           "etc2_srgb8",
                           tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8,
                       },
                       {
                           "GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2",
                           "etc2_rgb8_punchthrough_alpha1",
                           tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8_PUNCHTHROUGH_ALPHA1,
                       },
                       {
                           "GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2",
                           "etc2_srgb8_punchthrough_alpha1",
                           tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8_PUNCHTHROUGH_ALPHA1,
                       },
                       {
                           "GL_COMPRESSED_RGBA8_ETC2_EAC",
                           "etc2_eac_rgba8",
                           tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_RGBA8,
                       },
                       {
                           "GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC",
                           "etc2_eac_srgb8_alpha8",
                           tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_SRGB8_ALPHA8,
                       }};

    for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(etc2Formats); formatNdx++)
    {
        string descriptionBase = etc2Formats[formatNdx].descriptionBase;
        string nameBase        = etc2Formats[formatNdx].nameBase;

        compressedGroup->addChild(new Compressed2DFormatCase(
            m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), (nameBase + "_2d_pot").c_str(),
            (descriptionBase + ", GL_TEXTURE_2D").c_str(), etc2Formats[formatNdx].format, 1, 128, 64));
        compressedGroup->addChild(new CompressedCubeFormatCase(
            m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), (nameBase + "_cube_pot").c_str(),
            (descriptionBase + ", GL_TEXTURE_CUBE_MAP").c_str(), etc2Formats[formatNdx].format, 1, 64, 64));
        compressedGroup->addChild(new Compressed2DFormatCase(
            m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), (nameBase + "_2d_npot").c_str(),
            (descriptionBase + ", GL_TEXTURE_2D").c_str(), etc2Formats[formatNdx].format, 1, 51, 65));
        compressedGroup->addChild(new CompressedCubeFormatCase(
            m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), (nameBase + "_cube_npot").c_str(),
            (descriptionBase + ", GL_TEXTURE_CUBE_MAP").c_str(), etc2Formats[formatNdx].format, 1, 51, 51));
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
