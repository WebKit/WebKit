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
 * \brief Texture size tests.
 *//*--------------------------------------------------------------------*/

#include "es3fTextureSizeTests.hpp"
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
namespace gles3
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

class Texture2DSizeCase : public tcu::TestCase
{
public:
    Texture2DSizeCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                      const char *description, uint32_t format, uint32_t dataType, int width, int height, bool mipmaps);
    ~Texture2DSizeCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture2DSizeCase(const Texture2DSizeCase &other);
    Texture2DSizeCase &operator=(const Texture2DSizeCase &other);

    glu::RenderContext &m_renderCtx;

    uint32_t m_format;
    uint32_t m_dataType;
    int m_width;
    int m_height;
    bool m_useMipmaps;

    glu::Texture2D *m_texture;
    TextureRenderer m_renderer;
};

Texture2DSizeCase::Texture2DSizeCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                     const char *description, uint32_t format, uint32_t dataType, int width, int height,
                                     bool mipmaps)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_format(format)
    , m_dataType(dataType)
    , m_width(width)
    , m_height(height)
    , m_useMipmaps(mipmaps)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_MEDIUMP)
{
}

Texture2DSizeCase::~Texture2DSizeCase(void)
{
    Texture2DSizeCase::deinit();
}

void Texture2DSizeCase::init(void)
{
    DE_ASSERT(!m_texture);
    m_texture = new Texture2D(m_renderCtx, m_format, m_dataType, m_width, m_height);

    int numLevels = m_useMipmaps ? deLog2Floor32(de::max(m_width, m_height)) + 1 : 1;

    // Fill levels.
    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        m_texture->getRefTexture().allocLevel(levelNdx);
        tcu::fillWithComponentGradients(m_texture->getRefTexture().getLevel(levelNdx),
                                        tcu::Vec4(-1.0f, -1.0f, -1.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f));
    }
}

void Texture2DSizeCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

Texture2DSizeCase::IterateResult Texture2DSizeCase::iterate(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    TestLog &log             = m_testCtx.getLog();
    RandomViewport viewport(m_renderCtx.getRenderTarget(), 128, 128, deStringHash(getName()));
    tcu::Surface renderedFrame(viewport.width, viewport.height);
    tcu::Surface referenceFrame(viewport.width, viewport.height);
    const tcu::IVec4 texBits      = tcu::getTextureFormatBitDepth(glu::mapGLTransferFormat(m_format, m_dataType));
    const tcu::PixelFormat &rtFmt = m_renderCtx.getRenderTarget().getPixelFormat();
    const tcu::PixelFormat thresholdFormat(de::min(texBits[0], rtFmt.redBits), de::min(texBits[1], rtFmt.greenBits),
                                           de::min(texBits[2], rtFmt.blueBits), de::min(texBits[3], rtFmt.alphaBits));
    tcu::RGBA threshold = thresholdFormat.getColorThreshold() + tcu::RGBA(7, 7, 7, 7);
    uint32_t wrapS      = GL_CLAMP_TO_EDGE;
    uint32_t wrapT      = GL_CLAMP_TO_EDGE;
    // Do not minify with GL_NEAREST. A large POT texture with a small POT render target will produce
    // indeterminate results.
    uint32_t minFilter = m_useMipmaps ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR;
    uint32_t magFilter = GL_NEAREST;
    vector<float> texCoord;

    computeQuadTexCoord2D(texCoord, tcu::Vec2(0.0f, 0.0f), tcu::Vec2(1.0f, 1.0f));

    // Setup base viewport.
    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

    // Upload texture data to GL.
    m_texture->upload();

    // Bind to unit 0.
    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_2D, m_texture->getGLTexture());

    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Set texturing state");

    // Draw.
    m_renderer.renderQuad(0, &texCoord[0], TEXTURETYPE_2D);
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());

    // Compute reference.
    sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat()),
                  m_texture->getRefTexture(), &texCoord[0],
                  ReferenceParams(TEXTURETYPE_2D, mapGLSampler(wrapS, wrapT, minFilter, magFilter)));

    // Compare and log.
    bool isOk = compareImages(log, referenceFrame, renderedFrame, threshold);

    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                            isOk ? "Pass" : "Image comparison failed");

    return STOP;
}

class TextureCubeSizeCase : public tcu::TestCase
{
public:
    TextureCubeSizeCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                        const char *description, uint32_t format, uint32_t dataType, int width, int height,
                        bool mipmaps);
    ~TextureCubeSizeCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    TextureCubeSizeCase(const TextureCubeSizeCase &other);
    TextureCubeSizeCase &operator=(const TextureCubeSizeCase &other);

    bool testFace(tcu::CubeFace face);

    glu::RenderContext &m_renderCtx;

    uint32_t m_format;
    uint32_t m_dataType;
    int m_width;
    int m_height;
    bool m_useMipmaps;

    glu::TextureCube *m_texture;
    TextureRenderer m_renderer;

    int m_curFace;
    bool m_isOk;
};

TextureCubeSizeCase::TextureCubeSizeCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                         const char *description, uint32_t format, uint32_t dataType, int width,
                                         int height, bool mipmaps)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_format(format)
    , m_dataType(dataType)
    , m_width(width)
    , m_height(height)
    , m_useMipmaps(mipmaps)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_MEDIUMP)
    , m_curFace(0)
    , m_isOk(false)
{
}

TextureCubeSizeCase::~TextureCubeSizeCase(void)
{
    TextureCubeSizeCase::deinit();
}

void TextureCubeSizeCase::init(void)
{
    DE_ASSERT(!m_texture);
    DE_ASSERT(m_width == m_height);
    m_texture = new TextureCube(m_renderCtx, m_format, m_dataType, m_width);

    static const tcu::Vec4 gradients[tcu::CUBEFACE_LAST][2] = {
        {tcu::Vec4(-1.0f, -1.0f, -1.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)}, // negative x
        {tcu::Vec4(0.0f, -1.0f, -1.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)},  // positive x
        {tcu::Vec4(-1.0f, 0.0f, -1.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)},  // negative y
        {tcu::Vec4(-1.0f, -1.0f, 0.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)},  // positive y
        {tcu::Vec4(-1.0f, -1.0f, -1.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f)}, // negative z
        {tcu::Vec4(0.0f, 0.0f, 0.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)}     // positive z
    };

    int numLevels = m_useMipmaps ? deLog2Floor32(de::max(m_width, m_height)) + 1 : 1;

    // Fill levels.
    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
        {
            m_texture->getRefTexture().allocLevel((tcu::CubeFace)face, levelNdx);
            fillWithComponentGradients(m_texture->getRefTexture().getLevelFace(levelNdx, (tcu::CubeFace)face),
                                       gradients[face][0], gradients[face][1]);
        }
    }

    // Upload texture data to GL.
    m_texture->upload();

    // Initialize iteration state.
    m_curFace = 0;
    m_isOk    = true;
}

void TextureCubeSizeCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

bool TextureCubeSizeCase::testFace(tcu::CubeFace face)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    TestLog &log             = m_testCtx.getLog();
    RandomViewport viewport(m_renderCtx.getRenderTarget(), 128, 128, deStringHash(getName()) + (uint32_t)face);
    tcu::Surface renderedFrame(viewport.width, viewport.height);
    tcu::Surface referenceFrame(viewport.width, viewport.height);
    const tcu::IVec4 texBits      = tcu::getTextureFormatBitDepth(glu::mapGLTransferFormat(m_format, m_dataType));
    const tcu::PixelFormat &rtFmt = m_renderCtx.getRenderTarget().getPixelFormat();
    const tcu::PixelFormat thresholdFormat(de::min(texBits[0], rtFmt.redBits), de::min(texBits[1], rtFmt.greenBits),
                                           de::min(texBits[2], rtFmt.blueBits), de::min(texBits[3], rtFmt.alphaBits));
    tcu::RGBA threshold = thresholdFormat.getColorThreshold() + tcu::RGBA(7, 7, 7, 7);
    uint32_t wrapS      = GL_CLAMP_TO_EDGE;
    uint32_t wrapT      = GL_CLAMP_TO_EDGE;
    // Do not minify with GL_NEAREST. A large POT texture with a small POT render target will produce
    // indeterminate results.
    uint32_t minFilter = m_useMipmaps ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR;
    uint32_t magFilter = GL_NEAREST;
    vector<float> texCoord;

    computeQuadTexCoordCube(texCoord, face);

    // \todo [2011-10-28 pyry] Image set name / section?
    log << TestLog::Message << face << TestLog::EndMessage;

    // Setup base viewport.
    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

    // Bind to unit 0.
    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_CUBE_MAP, m_texture->getGLTexture());

    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, wrapS);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, wrapT);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, minFilter);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, magFilter);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Set texturing state");

    m_renderer.renderQuad(0, &texCoord[0], TEXTURETYPE_CUBE);
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());

    // Compute reference.
    Sampler sampler         = mapGLSampler(wrapS, wrapT, minFilter, magFilter);
    sampler.seamlessCubeMap = true;
    sampleTexture(tcu::SurfaceAccess(referenceFrame, m_renderCtx.getRenderTarget().getPixelFormat()),
                  m_texture->getRefTexture(), &texCoord[0], ReferenceParams(TEXTURETYPE_CUBE, sampler));

    // Compare and log.
    return compareImages(log, referenceFrame, renderedFrame, threshold);
}

TextureCubeSizeCase::IterateResult TextureCubeSizeCase::iterate(void)
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

TextureSizeTests::TextureSizeTests(Context &context) : TestCaseGroup(context, "size", "Texture Size Tests")
{
}

TextureSizeTests::~TextureSizeTests(void)
{
}

void TextureSizeTests::init(void)
{
    struct
    {
        int width;
        int height;
    } sizes2D[] = {{64, 64}, // Spec-mandated minimum.
                   {65, 63},
                   {512, 512},
                   {1024, 1024},
                   {2048, 2048}};

    struct
    {
        int width;
        int height;
    } sizesCube[] = {{15, 15}, {16, 16}, // Spec-mandated minimum
                     {64, 64}, {128, 128}, {256, 256}, {512, 512}};

    struct
    {
        const char *name;
        uint32_t format;
        uint32_t dataType;
    } formats[] = {{"l8", GL_LUMINANCE, GL_UNSIGNED_BYTE},
                   {"rgba4444", GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},
                   {"rgb888", GL_RGB, GL_UNSIGNED_BYTE},
                   {"rgba8888", GL_RGBA, GL_UNSIGNED_BYTE}};

    // 2D cases.
    tcu::TestCaseGroup *group2D = new tcu::TestCaseGroup(m_testCtx, "2d", "2D Texture Size Tests");
    addChild(group2D);
    for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes2D); sizeNdx++)
    {
        int width  = sizes2D[sizeNdx].width;
        int height = sizes2D[sizeNdx].height;
        bool isPOT = deIsPowerOfTwo32(width) && deIsPowerOfTwo32(height);

        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
        {
            for (int mipmap = 0; mipmap < (isPOT ? 2 : 1); mipmap++)
            {
                std::ostringstream name;
                name << width << "x" << height << "_" << formats[formatNdx].name << (mipmap ? "_mipmap" : "");

                group2D->addChild(new Texture2DSizeCase(m_testCtx, m_context.getRenderContext(), name.str().c_str(), "",
                                                        formats[formatNdx].format, formats[formatNdx].dataType, width,
                                                        height, mipmap != 0));
            }
        }
    }

    // Cubemap cases.
    tcu::TestCaseGroup *groupCube = new tcu::TestCaseGroup(m_testCtx, "cube", "Cubemap Texture Size Tests");
    addChild(groupCube);
    for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizesCube); sizeNdx++)
    {
        int width  = sizesCube[sizeNdx].width;
        int height = sizesCube[sizeNdx].height;
        bool isPOT = deIsPowerOfTwo32(width) && deIsPowerOfTwo32(height);

        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
        {
            for (int mipmap = 0; mipmap < (isPOT ? 2 : 1); mipmap++)
            {
                std::ostringstream name;
                name << width << "x" << height << "_" << formats[formatNdx].name << (mipmap ? "_mipmap" : "");

                groupCube->addChild(new TextureCubeSizeCase(m_testCtx, m_context.getRenderContext(), name.str().c_str(),
                                                            "", formats[formatNdx].format, formats[formatNdx].dataType,
                                                            width, height, mipmap != 0));
            }
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
