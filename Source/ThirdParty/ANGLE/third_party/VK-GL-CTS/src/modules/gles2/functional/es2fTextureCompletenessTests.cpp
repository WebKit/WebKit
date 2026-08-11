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
 * \brief Texture completeness tests.
 *//*--------------------------------------------------------------------*/

#include "es2fTextureCompletenessTests.hpp"
#include "glsTextureTestUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRenderTarget.hpp"

#include "deRandom.hpp"
#include "deInt32.h"
#include "deString.h"

#include "gluTextureUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluContextInfo.hpp"
#include "gluRenderContext.hpp"

#include "glw.h"

namespace deqp
{
namespace gles2
{
namespace Functional
{

using gls::TextureTestUtil::TextureRenderer;
using glu::TextureTestUtil::computeQuadTexCoord2D;
using glu::TextureTestUtil::computeQuadTexCoordCube;
using std::string;
using std::vector;
using tcu::IVec2;
using tcu::RGBA;
using tcu::Sampler;
using tcu::TestLog;
using tcu::TextureFormat;

static const GLenum s_cubeTargets[] = {GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                                       GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
                                       GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, GL_TEXTURE_CUBE_MAP_POSITIVE_Z};

static bool isExtensionSupported(const glu::ContextInfo &ctxInfo, const char *extension)
{
    vector<string> extensions = ctxInfo.getExtensions();

    for (vector<string>::iterator iter = extensions.begin(); iter != extensions.end(); ++iter)
        if (iter->compare(extension) == 0)
            return true;

    return false;
}

static bool compareToConstantColor(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                                   const tcu::Surface &result, tcu::CompareLogMode logMode, RGBA color)
{
    bool isOk = true;

    for (int y = 0; y < result.getHeight(); y++)
    {
        for (int x = 0; x < result.getWidth(); x++)
        {
            if (result.getPixel(x, y).getRed() != color.getRed() ||
                result.getPixel(x, y).getGreen() != color.getGreen() ||
                result.getPixel(x, y).getBlue() != color.getBlue() ||
                result.getPixel(x, y).getAlpha() != color.getAlpha())
            {
                isOk = false;
            }
        }
    }

    if (!isOk || logMode == tcu::COMPARE_LOG_EVERYTHING)
    {
        if (!isOk)
            log << TestLog::Message << "Image comparison failed" << TestLog::EndMessage;

        log << TestLog::ImageSet(imageSetName, imageSetDesc) << TestLog::Image("Result", "Result", result)
            << TestLog::EndImageSet;
    }
    else if (logMode == tcu::COMPARE_LOG_RESULT)
        log << TestLog::ImageSet(imageSetName, imageSetDesc) << TestLog::Image("Result", "Result", result)
            << TestLog::EndImageSet;

    return isOk;
}

// Base classes.

class Tex2DCompletenessCase : public tcu::TestCase
{
public:
    Tex2DCompletenessCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                          const char *description);
    ~Tex2DCompletenessCase(void)
    {
    }

    IterateResult iterate(void);

protected:
    virtual void createTexture(GLuint texture) = 0;

    tcu::TestContext &m_testCtx;
    glu::RenderContext &m_renderCtx;
    RGBA m_compareColor;
};

Tex2DCompletenessCase::Tex2DCompletenessCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                             const char *description)
    : TestCase(testCtx, name, description)
    , m_testCtx(testCtx)
    , m_renderCtx(renderCtx)
    , m_compareColor(RGBA(0, 0, 0, 255))
{
}

Tex2DCompletenessCase::IterateResult Tex2DCompletenessCase::iterate(void)
{
    int viewportWidth  = de::min(64, m_renderCtx.getRenderTarget().getWidth());
    int viewportHeight = de::min(64, m_renderCtx.getRenderTarget().getHeight());
    TestLog &log       = m_testCtx.getLog();
    TextureRenderer renderer(m_renderCtx, log, glu::GLSL_VERSION_100_ES, glu::PRECISION_MEDIUMP);
    tcu::Surface renderedFrame(viewportWidth, viewportHeight);
    vector<float> texCoord;

    de::Random random(deStringHash(getName()));
    int offsetX = random.getInt(0, m_renderCtx.getRenderTarget().getWidth() - viewportWidth);
    int offsetY = random.getInt(0, m_renderCtx.getRenderTarget().getHeight() - viewportHeight);

    computeQuadTexCoord2D(texCoord, tcu::Vec2(0.0f, 0.0f), tcu::Vec2(1.0f, 1.0f));

    glViewport(offsetX, offsetY, viewportWidth, viewportHeight);

    GLuint texture;
    glGenTextures(1, &texture);
    createTexture(texture);

    renderer.renderQuad(0, &texCoord[0], glu::TextureTestUtil::TEXTURETYPE_2D);
    glu::readPixels(m_renderCtx, offsetX, offsetY, renderedFrame.getAccess());

    bool isOk = compareToConstantColor(log, "Result", "Image comparison result", renderedFrame, tcu::COMPARE_LOG_RESULT,
                                       m_compareColor);

    glDeleteTextures(1, &texture);

    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                            isOk ? "Pass" : "Image comparison failed");
    return STOP;
}

class TexCubeCompletenessCase : public tcu::TestCase
{
public:
    TexCubeCompletenessCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                            const char *description);
    ~TexCubeCompletenessCase(void)
    {
    }

    IterateResult iterate(void);

protected:
    virtual void createTexture(GLuint texture) = 0;

    tcu::TestContext &m_testCtx;
    glu::RenderContext &m_renderCtx;
    RGBA m_compareColor;
};

TexCubeCompletenessCase::TexCubeCompletenessCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                 const char *name, const char *description)
    : TestCase(testCtx, name, description)
    , m_testCtx(testCtx)
    , m_renderCtx(renderCtx)
    , m_compareColor(RGBA(0, 0, 0, 255))
{
}

TexCubeCompletenessCase::IterateResult TexCubeCompletenessCase::iterate(void)
{
    int viewportWidth  = de::min(64, m_renderCtx.getRenderTarget().getWidth());
    int viewportHeight = de::min(64, m_renderCtx.getRenderTarget().getHeight());
    bool allFacesOk    = true;
    TestLog &log       = m_testCtx.getLog();
    TextureRenderer renderer(m_renderCtx, log, glu::GLSL_VERSION_100_ES, glu::PRECISION_MEDIUMP);
    tcu::Surface renderedFrame(viewportWidth, viewportHeight);
    vector<float> texCoord;

    de::Random random(deStringHash(getName()));
    int offsetX = random.getInt(0, de::max(0, m_renderCtx.getRenderTarget().getWidth() - 64));
    int offsetY = random.getInt(0, de::max(0, m_renderCtx.getRenderTarget().getHeight() - 64));

    GLuint texture;
    glGenTextures(1, &texture);
    createTexture(texture);

    for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
    {
        computeQuadTexCoordCube(texCoord, (tcu::CubeFace)face);

        glViewport(offsetX, offsetY, viewportWidth, viewportHeight);

        renderer.renderQuad(0, &texCoord[0], glu::TextureTestUtil::TEXTURETYPE_CUBE);
        glu::readPixels(m_renderCtx, offsetX, offsetY, renderedFrame.getAccess());

        bool isOk = compareToConstantColor(log, "Result", "Image comparison result", renderedFrame,
                                           tcu::COMPARE_LOG_RESULT, m_compareColor);

        if (!isOk)
            allFacesOk = false;
    }

    glDeleteTextures(1, &texture);

    m_testCtx.setTestResult(allFacesOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                            allFacesOk ? "Pass" : "Image comparison failed");
    return STOP;
}

// Texture 2D tests.

class Incomplete2DSizeCase : public Tex2DCompletenessCase
{
public:
    Incomplete2DSizeCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                         const char *description, IVec2 size, IVec2 invalidLevelSize, int invalidLevelNdx,
                         const glu::ContextInfo &ctxInfo);
    ~Incomplete2DSizeCase(void)
    {
    }

    virtual void createTexture(GLuint texture);

private:
    int m_invalidLevelNdx;
    IVec2 m_invalidLevelSize;
    const glu::ContextInfo &m_ctxInfo;
    IVec2 m_size;
};

Incomplete2DSizeCase::Incomplete2DSizeCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                           const char *description, IVec2 size, IVec2 invalidLevelSize,
                                           int invalidLevelNdx, const glu::ContextInfo &ctxInfo)
    : Tex2DCompletenessCase(testCtx, renderCtx, name, description)
    , m_invalidLevelNdx(invalidLevelNdx)
    , m_invalidLevelSize(invalidLevelSize)
    , m_ctxInfo(ctxInfo)
    , m_size(size)
{
}

void Incomplete2DSizeCase::createTexture(GLuint texture)
{
    static const char *const s_relaxingExtensions[] = {
        "GL_OES_texture_npot",
        "GL_NV_texture_npot_2D_mipmap",
    };

    tcu::TextureFormat fmt = glu::mapGLTransferFormat(GL_RGBA, GL_UNSIGNED_BYTE);
    tcu::TextureLevel levelData(fmt);
    TestLog &log = m_testCtx.getLog();

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int numLevels = 1 + de::max(deLog2Floor32(m_size.x()), deLog2Floor32(m_size.y()));

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        int levelW = (levelNdx == m_invalidLevelNdx) ? m_invalidLevelSize.x() : de::max(1, m_size.x() >> levelNdx);
        int levelH = (levelNdx == m_invalidLevelNdx) ? m_invalidLevelSize.y() : de::max(1, m_size.y() >> levelNdx);

        levelData.setSize(m_size.x(), m_size.y());
        clear(levelData.getAccess(), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

        glTexImage2D(GL_TEXTURE_2D, levelNdx, GL_RGBA, levelW, levelH, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     levelData.getAccess().getDataPtr());
    }

    GLU_CHECK_MSG("Set texturing state");

    // If size not allowed in core, search for relaxing extensions
    if (!deIsPowerOfTwo32(m_size.x()) && !deIsPowerOfTwo32(m_size.y()))
    {
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_relaxingExtensions); ++ndx)
        {
            if (isExtensionSupported(m_ctxInfo, s_relaxingExtensions[ndx]))
            {
                log << TestLog::Message << s_relaxingExtensions[ndx]
                    << " supported, assuming completeness test to pass." << TestLog::EndMessage;
                m_compareColor = RGBA(0, 0, 255, 255);
                break;
            }
        }
    }
}

class Incomplete2DFormatCase : public Tex2DCompletenessCase
{
public:
    Incomplete2DFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                           const char *description, IVec2 size, uint32_t format, uint32_t invalidFormat,
                           int invalidLevelNdx);
    ~Incomplete2DFormatCase(void)
    {
    }

    virtual void createTexture(GLuint texture);

private:
    int m_invalidLevelNdx;
    uint32_t m_format;
    uint32_t m_invalidFormat;
    IVec2 m_size;
};

Incomplete2DFormatCase::Incomplete2DFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                               const char *name, const char *description, IVec2 size, uint32_t format,
                                               uint32_t invalidFormat, int invalidLevelNdx)
    : Tex2DCompletenessCase(testCtx, renderCtx, name, description)
    , m_invalidLevelNdx(invalidLevelNdx)
    , m_format(format)
    , m_invalidFormat(invalidFormat)
    , m_size(size)
{
}

void Incomplete2DFormatCase::createTexture(GLuint texture)
{
    tcu::TextureFormat fmt = glu::mapGLTransferFormat(m_format, GL_UNSIGNED_BYTE);
    tcu::TextureLevel levelData(fmt);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int numLevels = 1 + de::max(deLog2Floor32(m_size.x()), deLog2Floor32(m_size.y()));

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        int levelW = de::max(1, m_size.x() >> levelNdx);
        int levelH = de::max(1, m_size.y() >> levelNdx);

        levelData.setSize(m_size.x(), m_size.y());
        clear(levelData.getAccess(), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

        uint32_t format = levelNdx == m_invalidLevelNdx ? m_invalidFormat : m_format;

        glTexImage2D(GL_TEXTURE_2D, levelNdx, format, levelW, levelH, 0, format, GL_UNSIGNED_BYTE,
                     levelData.getAccess().getDataPtr());
    }

    GLU_CHECK_MSG("Set texturing state");
}

class Incomplete2DMissingLevelCase : public Tex2DCompletenessCase
{
public:
    Incomplete2DMissingLevelCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                 const char *description, IVec2 size, int missingLevelNdx);
    ~Incomplete2DMissingLevelCase(void)
    {
    }

    virtual void createTexture(GLuint texture);

private:
    int m_missingLevelNdx;
    IVec2 m_size;
};

Incomplete2DMissingLevelCase::Incomplete2DMissingLevelCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                           const char *name, const char *description, IVec2 size,
                                                           int missingLevelNdx)
    : Tex2DCompletenessCase(testCtx, renderCtx, name, description)
    , m_missingLevelNdx(missingLevelNdx)
    , m_size(size)
{
}

void Incomplete2DMissingLevelCase::createTexture(GLuint texture)
{
    tcu::TextureFormat fmt = glu::mapGLTransferFormat(GL_RGBA, GL_UNSIGNED_BYTE);
    tcu::TextureLevel levelData(fmt);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int numLevels = 1 + de::max(deLog2Floor32(m_size.x()), deLog2Floor32(m_size.y()));

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        levelData.setSize(m_size.x(), m_size.y());
        clear(levelData.getAccess(), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

        int levelW = de::max(1, m_size.x() >> levelNdx);
        int levelH = de::max(1, m_size.y() >> levelNdx);

        // Skip specified level.
        if (levelNdx != m_missingLevelNdx)
            glTexImage2D(GL_TEXTURE_2D, levelNdx, GL_RGBA, levelW, levelH, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         levelData.getAccess().getDataPtr());
    }

    GLU_CHECK_MSG("Set texturing state");
}

class Incomplete2DWrapModeCase : public Tex2DCompletenessCase
{
public:
    Incomplete2DWrapModeCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                             const char *description, IVec2 size, uint32_t wrapT, uint32_t wrapS,
                             const glu::ContextInfo &ctxInfo);
    ~Incomplete2DWrapModeCase(void)
    {
    }

    virtual void createTexture(GLuint texture);

private:
    uint32_t m_wrapT;
    uint32_t m_wrapS;
    const glu::ContextInfo &m_ctxInfo;
    IVec2 m_size;
};

Incomplete2DWrapModeCase::Incomplete2DWrapModeCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                   const char *name, const char *description, IVec2 size,
                                                   uint32_t wrapT, uint32_t wrapS, const glu::ContextInfo &ctxInfo)
    : Tex2DCompletenessCase(testCtx, renderCtx, name, description)
    , m_wrapT(wrapT)
    , m_wrapS(wrapS)
    , m_ctxInfo(ctxInfo)
    , m_size(size)
{
}

void Incomplete2DWrapModeCase::createTexture(GLuint texture)
{
    TestLog &log           = m_testCtx.getLog();
    tcu::TextureFormat fmt = glu::mapGLTransferFormat(GL_RGBA, GL_UNSIGNED_BYTE);
    tcu::TextureLevel levelData(fmt);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    levelData.setSize(m_size.x(), m_size.y());
    clear(levelData.getAccess(), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_size.x(), m_size.y(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 levelData.getAccess().getDataPtr());

    GLU_CHECK_MSG("Set texturing state");

    const char *extension = "GL_OES_texture_npot";
    if (isExtensionSupported(m_ctxInfo, extension))
    {
        log << TestLog::Message << extension << " supported, assuming completeness test to pass."
            << TestLog::EndMessage;
        m_compareColor = RGBA(0, 0, 255, 255);
    }
}

class Complete2DExtraLevelCase : public Tex2DCompletenessCase
{
public:
    Complete2DExtraLevelCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                             const char *description, IVec2 size);
    ~Complete2DExtraLevelCase(void)
    {
    }

    virtual void createTexture(GLuint texture);

private:
    IVec2 m_size;
};

Complete2DExtraLevelCase::Complete2DExtraLevelCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                   const char *name, const char *description, IVec2 size)
    : Tex2DCompletenessCase(testCtx, renderCtx, name, description)
    , m_size(size)
{
}

void Complete2DExtraLevelCase::createTexture(GLuint texture)
{
    tcu::TextureFormat fmt = glu::mapGLTransferFormat(GL_RGBA, GL_UNSIGNED_BYTE);
    tcu::TextureLevel levelData(fmt);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int numLevels = 1 + de::max(deLog2Floor32(m_size.x()), deLog2Floor32(m_size.y()));

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        int levelW = de::max(1, m_size.x() >> levelNdx);
        int levelH = de::max(1, m_size.y() >> levelNdx);

        levelData.setSize(m_size.x(), m_size.y());
        clear(levelData.getAccess(), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

        glTexImage2D(GL_TEXTURE_2D, levelNdx, GL_RGBA, levelW, levelH, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     levelData.getAccess().getDataPtr());
    }

    // Specify extra level.
    glTexImage2D(GL_TEXTURE_2D, numLevels + 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 levelData.getAccess().getDataPtr());
    m_compareColor = RGBA(0, 0, 255, 255);

    GLU_CHECK_MSG("Set texturing state");
}

class Incomplete2DEmptyObjectCase : public Tex2DCompletenessCase
{
public:
    Incomplete2DEmptyObjectCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                const char *description);
    ~Incomplete2DEmptyObjectCase(void)
    {
    }

    virtual void createTexture(GLuint texture);
};

Incomplete2DEmptyObjectCase::Incomplete2DEmptyObjectCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                         const char *name, const char *description)
    : Tex2DCompletenessCase(testCtx, renderCtx, name, description)
{
}

void Incomplete2DEmptyObjectCase::createTexture(GLuint texture)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLU_CHECK_MSG("Set texturing state");
}

// Cube texture tests.

class IncompleteCubeSizeCase : public TexCubeCompletenessCase
{
public:
    IncompleteCubeSizeCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                           const char *description, IVec2 size, IVec2 invalidLevelSize, int invalidLevelNdx);
    IncompleteCubeSizeCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                           const char *description, IVec2 size, IVec2 invalidLevelSize, int invalidLevelNdx,
                           tcu::CubeFace invalidCubeFace);
    ~IncompleteCubeSizeCase(void)
    {
    }

    virtual void createTexture(GLuint texture);

private:
    int m_invalidLevelNdx;
    IVec2 m_invalidLevelSize;
    tcu::CubeFace m_invalidCubeFace;
    IVec2 m_size;
};

IncompleteCubeSizeCase::IncompleteCubeSizeCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                               const char *name, const char *description, IVec2 size,
                                               IVec2 invalidLevelSize, int invalidLevelNdx)
    : TexCubeCompletenessCase(testCtx, renderCtx, name, description)
    , m_invalidLevelNdx(invalidLevelNdx)
    , m_invalidLevelSize(invalidLevelSize)
    , m_invalidCubeFace(tcu::CUBEFACE_LAST)
    , m_size(size)
{
}

IncompleteCubeSizeCase::IncompleteCubeSizeCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                               const char *name, const char *description, IVec2 size,
                                               IVec2 invalidLevelSize, int invalidLevelNdx,
                                               tcu::CubeFace invalidCubeFace)
    : TexCubeCompletenessCase(testCtx, renderCtx, name, description)
    , m_invalidLevelNdx(invalidLevelNdx)
    , m_invalidLevelSize(invalidLevelSize)
    , m_invalidCubeFace(invalidCubeFace)
    , m_size(size)
{
}

void IncompleteCubeSizeCase::createTexture(GLuint texture)
{
    tcu::TextureFormat fmt = glu::mapGLTransferFormat(GL_RGBA, GL_UNSIGNED_BYTE);
    tcu::TextureLevel levelData(fmt);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int numLevels = 1 + de::max(deLog2Floor32(m_size.x()), deLog2Floor32(m_size.y()));

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        levelData.setSize(m_size.x(), m_size.y());
        clear(levelData.getAccess(), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

        for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(s_cubeTargets); targetNdx++)
        {
            int levelW = de::max(1, m_size.x() >> levelNdx);
            int levelH = de::max(1, m_size.y() >> levelNdx);
            if (levelNdx == m_invalidLevelNdx &&
                (m_invalidCubeFace == tcu::CUBEFACE_LAST || m_invalidCubeFace == targetNdx))
            {
                levelW = m_invalidLevelSize.x();
                levelH = m_invalidLevelSize.y();
            }
            glTexImage2D(s_cubeTargets[targetNdx], levelNdx, GL_RGBA, levelW, levelH, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         levelData.getAccess().getDataPtr());
        }
    }

    GLU_CHECK_MSG("Set texturing state");
}

class IncompleteCubeFormatCase : public TexCubeCompletenessCase
{
public:
    IncompleteCubeFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                             const char *description, IVec2 size, uint32_t format, uint32_t invalidFormat);
    IncompleteCubeFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                             const char *description, IVec2 size, uint32_t format, uint32_t invalidFormat,
                             tcu::CubeFace invalidCubeFace);
    ~IncompleteCubeFormatCase(void)
    {
    }

    virtual void createTexture(GLuint texture);

private:
    uint32_t m_format;
    uint32_t m_invalidFormat;
    tcu::CubeFace m_invalidCubeFace;
    IVec2 m_size;
};

IncompleteCubeFormatCase::IncompleteCubeFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                   const char *name, const char *description, IVec2 size,
                                                   uint32_t format, uint32_t invalidFormat)
    : TexCubeCompletenessCase(testCtx, renderCtx, name, description)
    , m_format(format)
    , m_invalidFormat(invalidFormat)
    , m_invalidCubeFace(tcu::CUBEFACE_LAST)
    , m_size(size)
{
}

IncompleteCubeFormatCase::IncompleteCubeFormatCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                   const char *name, const char *description, IVec2 size,
                                                   uint32_t format, uint32_t invalidFormat,
                                                   tcu::CubeFace invalidCubeFace)
    : TexCubeCompletenessCase(testCtx, renderCtx, name, description)
    , m_format(format)
    , m_invalidFormat(invalidFormat)
    , m_invalidCubeFace(invalidCubeFace)
    , m_size(size)
{
}

void IncompleteCubeFormatCase::createTexture(GLuint texture)
{
    tcu::TextureFormat fmt = glu::mapGLTransferFormat(GL_RGBA, GL_UNSIGNED_BYTE);
    tcu::TextureLevel levelData(fmt);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int numLevels = 1 + de::max(deLog2Floor32(m_size.x()), deLog2Floor32(m_size.y()));

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        levelData.setSize(m_size.x(), m_size.y());
        clear(levelData.getAccess(), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

        int levelW = de::max(1, m_size.x() >> levelNdx);
        int levelH = de::max(1, m_size.y() >> levelNdx);

        for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(s_cubeTargets); targetNdx++)
        {
            uint32_t format = m_format;
            if (levelNdx == 0 && (m_invalidCubeFace == tcu::CUBEFACE_LAST || m_invalidCubeFace == targetNdx))
                format = m_invalidFormat;

            glTexImage2D(s_cubeTargets[targetNdx], levelNdx, format, levelW, levelH, 0, format, GL_UNSIGNED_BYTE,
                         levelData.getAccess().getDataPtr());
        }
    }

    GLU_CHECK_MSG("Set texturing state");
}

class IncompleteCubeMissingLevelCase : public TexCubeCompletenessCase
{
public:
    IncompleteCubeMissingLevelCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                   const char *description, IVec2 size, int invalidLevelNdx);
    IncompleteCubeMissingLevelCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                   const char *description, IVec2 size, int invalidLevelNdx,
                                   tcu::CubeFace invalidCubeFace);
    ~IncompleteCubeMissingLevelCase(void)
    {
    }

    virtual void createTexture(GLuint texture);

private:
    int m_invalidLevelNdx;
    tcu::CubeFace m_invalidCubeFace;
    IVec2 m_size;
};

IncompleteCubeMissingLevelCase::IncompleteCubeMissingLevelCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                               const char *name, const char *description, IVec2 size,
                                                               int invalidLevelNdx)
    : TexCubeCompletenessCase(testCtx, renderCtx, name, description)
    , m_invalidLevelNdx(invalidLevelNdx)
    , m_invalidCubeFace(tcu::CUBEFACE_LAST)
    , m_size(size)
{
}

IncompleteCubeMissingLevelCase::IncompleteCubeMissingLevelCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                               const char *name, const char *description, IVec2 size,
                                                               int invalidLevelNdx, tcu::CubeFace invalidCubeFace)
    : TexCubeCompletenessCase(testCtx, renderCtx, name, description)
    , m_invalidLevelNdx(invalidLevelNdx)
    , m_invalidCubeFace(invalidCubeFace)
    , m_size(size)
{
}

void IncompleteCubeMissingLevelCase::createTexture(GLuint texture)
{
    tcu::TextureFormat fmt = glu::mapGLTransferFormat(GL_RGBA, GL_UNSIGNED_BYTE);
    tcu::TextureLevel levelData(fmt);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int numLevels = 1 + de::max(deLog2Floor32(m_size.x()), deLog2Floor32(m_size.y()));

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        levelData.setSize(m_size.x(), m_size.y());
        clear(levelData.getAccess(), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

        int levelW = (levelNdx == m_invalidLevelNdx) ? m_size.x() : de::max(1, m_size.x() >> levelNdx);
        int levelH = (levelNdx == m_invalidLevelNdx) ? m_size.y() : de::max(1, m_size.y() >> levelNdx);

        if (levelNdx != m_invalidLevelNdx || m_invalidCubeFace != tcu::CUBEFACE_LAST)
        {
            for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(s_cubeTargets); targetNdx++)
            {
                // If single cubeface is specified then skip only that one.
                if (m_invalidCubeFace != targetNdx)
                    glTexImage2D(s_cubeTargets[targetNdx], levelNdx, GL_RGBA, levelW, levelH, 0, GL_RGBA,
                                 GL_UNSIGNED_BYTE, levelData.getAccess().getDataPtr());
            }
        }
    }

    GLU_CHECK_MSG("Set texturing state");
}

class IncompleteCubeWrapModeCase : public TexCubeCompletenessCase
{
public:
    IncompleteCubeWrapModeCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                               const char *description, IVec2 size, uint32_t wrapT, uint32_t wrapS,
                               const glu::ContextInfo &ctxInfo);
    ~IncompleteCubeWrapModeCase(void)
    {
    }

    virtual void createTexture(GLuint texture);

private:
    uint32_t m_wrapT;
    uint32_t m_wrapS;
    const glu::ContextInfo &m_ctxInfo;
    IVec2 m_size;
};

IncompleteCubeWrapModeCase::IncompleteCubeWrapModeCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                       const char *name, const char *description, IVec2 size,
                                                       uint32_t wrapT, uint32_t wrapS, const glu::ContextInfo &ctxInfo)
    : TexCubeCompletenessCase(testCtx, renderCtx, name, description)
    , m_wrapT(wrapT)
    , m_wrapS(wrapS)
    , m_ctxInfo(ctxInfo)
    , m_size(size)
{
}

void IncompleteCubeWrapModeCase::createTexture(GLuint texture)
{
    TestLog &log           = m_testCtx.getLog();
    tcu::TextureFormat fmt = glu::mapGLTransferFormat(GL_RGBA, GL_UNSIGNED_BYTE);
    tcu::TextureLevel levelData(fmt);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, m_wrapS);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, m_wrapT);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    levelData.setSize(m_size.x(), m_size.y());
    clear(levelData.getAccess(), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

    for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(s_cubeTargets); targetNdx++)
        glTexImage2D(s_cubeTargets[targetNdx], 0, GL_RGBA, m_size.x(), m_size.y(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     levelData.getAccess().getDataPtr());

    GLU_CHECK_MSG("Set texturing state");

    const char *extension = "GL_OES_texture_npot";
    if (isExtensionSupported(m_ctxInfo, extension))
    {
        log << TestLog::Message << extension << " supported, assuming completeness test to pass."
            << TestLog::EndMessage;
        m_compareColor = RGBA(0, 0, 255, 255);
    }
}

class CompleteCubeExtraLevelCase : public TexCubeCompletenessCase
{
public:
    CompleteCubeExtraLevelCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                               const char *description, IVec2 size);
    ~CompleteCubeExtraLevelCase(void)
    {
    }

    virtual void createTexture(GLuint texture);

private:
    IVec2 m_size;
};

CompleteCubeExtraLevelCase::CompleteCubeExtraLevelCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                       const char *name, const char *description, IVec2 size)
    : TexCubeCompletenessCase(testCtx, renderCtx, name, description)
    , m_size(size)
{
}

void CompleteCubeExtraLevelCase::createTexture(GLuint texture)
{
    tcu::TextureFormat fmt = glu::mapGLTransferFormat(GL_RGBA, GL_UNSIGNED_BYTE);
    tcu::TextureLevel levelData(fmt);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int numLevels = 1 + de::max(deLog2Floor32(m_size.x()), deLog2Floor32(m_size.y()));

    levelData.setSize(m_size.x(), m_size.y());
    clear(levelData.getAccess(), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        int levelW = de::max(1, m_size.x() >> levelNdx);
        int levelH = de::max(1, m_size.y() >> levelNdx);

        for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(s_cubeTargets); targetNdx++)
            glTexImage2D(s_cubeTargets[targetNdx], levelNdx, GL_RGBA, levelW, levelH, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         levelData.getAccess().getDataPtr());
    }

    // Specify extra level.
    for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(s_cubeTargets); targetNdx++)
        glTexImage2D(s_cubeTargets[targetNdx], numLevels + 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     levelData.getAccess().getDataPtr());

    m_compareColor = RGBA(0, 0, 255, 255);

    GLU_CHECK_MSG("Set texturing state");
}

class IncompleteCubeEmptyObjectCase : public TexCubeCompletenessCase
{
public:
    IncompleteCubeEmptyObjectCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                  const char *description);
    ~IncompleteCubeEmptyObjectCase(void)
    {
    }

    virtual void createTexture(GLuint texture);
};

IncompleteCubeEmptyObjectCase::IncompleteCubeEmptyObjectCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                             const char *name, const char *description)
    : TexCubeCompletenessCase(testCtx, renderCtx, name, description)
{
}

void IncompleteCubeEmptyObjectCase::createTexture(GLuint texture)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLU_CHECK_MSG("Set texturing state");
}

// Texture completeness group.

TextureCompletenessTests::TextureCompletenessTests(Context &context)
    : TestCaseGroup(context, "completeness", "Completeness tests")
{
}

void TextureCompletenessTests::init(void)
{
    tcu::TestCaseGroup *tex2d = new tcu::TestCaseGroup(m_testCtx, "2d", "2D completeness");
    addChild(tex2d);
    tcu::TestCaseGroup *cube = new tcu::TestCaseGroup(m_testCtx, "cube", "Cubemap completeness");
    addChild(cube);

    // Texture 2D size.
    tex2d->addChild(new Incomplete2DSizeCase(m_testCtx, m_context.getRenderContext(), "npot_size", "", IVec2(255, 255),
                                             IVec2(255, 255), 0, m_context.getContextInfo()));
    tex2d->addChild(new Incomplete2DSizeCase(m_testCtx, m_context.getRenderContext(), "npot_size_level_0", "",
                                             IVec2(256, 256), IVec2(255, 255), 0, m_context.getContextInfo()));
    tex2d->addChild(new Incomplete2DSizeCase(m_testCtx, m_context.getRenderContext(), "npot_size_level_1", "",
                                             IVec2(256, 256), IVec2(127, 127), 1, m_context.getContextInfo()));
    tex2d->addChild(new Incomplete2DSizeCase(m_testCtx, m_context.getRenderContext(), "not_positive_level_0", "",
                                             IVec2(256, 256), IVec2(0, 0), 0, m_context.getContextInfo()));
    // Texture 2D format.
    tex2d->addChild(new Incomplete2DFormatCase(m_testCtx, m_context.getRenderContext(), "format_mismatch_rgb_rgba", "",
                                               IVec2(128, 128), GL_RGB, GL_RGBA, 1));
    tex2d->addChild(new Incomplete2DFormatCase(m_testCtx, m_context.getRenderContext(), "format_mismatch_rgba_rgb", "",
                                               IVec2(128, 128), GL_RGBA, GL_RGB, 1));
    tex2d->addChild(new Incomplete2DFormatCase(m_testCtx, m_context.getRenderContext(),
                                               "format_mismatch_luminance_luminance_alpha", "", IVec2(128, 128),
                                               GL_LUMINANCE, GL_LUMINANCE_ALPHA, 1));
    tex2d->addChild(new Incomplete2DFormatCase(m_testCtx, m_context.getRenderContext(),
                                               "format_mismatch_luminance_alpha_luminance", "", IVec2(128, 128),
                                               GL_LUMINANCE_ALPHA, GL_LUMINANCE, 1));
    // Texture 2D missing level.
    tex2d->addChild(new Incomplete2DMissingLevelCase(m_testCtx, m_context.getRenderContext(), "missing_level_1", "",
                                                     IVec2(128, 128), 1));
    tex2d->addChild(new Incomplete2DMissingLevelCase(m_testCtx, m_context.getRenderContext(), "missing_level_3", "",
                                                     IVec2(128, 128), 3));
    tex2d->addChild(new Incomplete2DMissingLevelCase(m_testCtx, m_context.getRenderContext(), "last_level_missing", "",
                                                     IVec2(128, 64), de::max(deLog2Floor32(128), deLog2Floor32(64))));
    // Texture 2D wrap modes.
    tex2d->addChild(new Incomplete2DWrapModeCase(m_testCtx, m_context.getRenderContext(), "npot_t_repeat", "",
                                                 IVec2(127, 127), GL_CLAMP_TO_EDGE, GL_REPEAT,
                                                 m_context.getContextInfo()));
    tex2d->addChild(new Incomplete2DWrapModeCase(m_testCtx, m_context.getRenderContext(), "npot_s_repeat", "",
                                                 IVec2(127, 127), GL_REPEAT, GL_CLAMP_TO_EDGE,
                                                 m_context.getContextInfo()));
    tex2d->addChild(new Incomplete2DWrapModeCase(m_testCtx, m_context.getRenderContext(), "npot_all_repeat", "",
                                                 IVec2(127, 127), GL_REPEAT, GL_REPEAT, m_context.getContextInfo()));
    tex2d->addChild(new Incomplete2DWrapModeCase(m_testCtx, m_context.getRenderContext(), "npot_mirrored_repeat", "",
                                                 IVec2(127, 127), GL_MIRRORED_REPEAT, GL_MIRRORED_REPEAT,
                                                 m_context.getContextInfo()));
    tex2d->addChild(new Incomplete2DWrapModeCase(m_testCtx, m_context.getRenderContext(), "repeat_width_npot", "",
                                                 IVec2(127, 128), GL_REPEAT, GL_REPEAT, m_context.getContextInfo()));
    tex2d->addChild(new Incomplete2DWrapModeCase(m_testCtx, m_context.getRenderContext(), "repeat_height_npot", "",
                                                 IVec2(128, 127), GL_REPEAT, GL_REPEAT, m_context.getContextInfo()));
    // Texture 2D extra level.
    tex2d->addChild(
        new Complete2DExtraLevelCase(m_testCtx, m_context.getRenderContext(), "extra_level", "", IVec2(64, 64)));
    // Texture 2D empty object.
    tex2d->addChild(new Incomplete2DEmptyObjectCase(m_testCtx, m_context.getRenderContext(), "empty_object", ""));

    // Cube size.
    cube->addChild(new IncompleteCubeSizeCase(m_testCtx, m_context.getRenderContext(), "npot_size_level_0", "",
                                              IVec2(64, 64), IVec2(63, 63), 0));
    cube->addChild(new IncompleteCubeSizeCase(m_testCtx, m_context.getRenderContext(), "npot_size_level_1", "",
                                              IVec2(64, 64), IVec2(31, 31), 1));
    cube->addChild(new IncompleteCubeSizeCase(m_testCtx, m_context.getRenderContext(), "npot_size_level_0_pos_x", "",
                                              IVec2(64, 64), IVec2(63, 63), 0, tcu::CUBEFACE_POSITIVE_X));
    cube->addChild(new IncompleteCubeSizeCase(m_testCtx, m_context.getRenderContext(), "npot_size_level_1_neg_x", "",
                                              IVec2(64, 64), IVec2(31, 31), 1, tcu::CUBEFACE_NEGATIVE_X));
    cube->addChild(new IncompleteCubeSizeCase(m_testCtx, m_context.getRenderContext(), "not_positive_level_0", "",
                                              IVec2(64, 64), IVec2(0, 0), 0));
    // Cube format.
    cube->addChild(new IncompleteCubeFormatCase(m_testCtx, m_context.getRenderContext(),
                                                "format_mismatch_rgb_rgba_level_0", "", IVec2(64, 64), GL_RGB,
                                                GL_RGBA));
    cube->addChild(new IncompleteCubeFormatCase(m_testCtx, m_context.getRenderContext(),
                                                "format_mismatch_rgba_rgb_level_0", "", IVec2(64, 64), GL_RGBA,
                                                GL_RGB));
    cube->addChild(new IncompleteCubeFormatCase(m_testCtx, m_context.getRenderContext(),
                                                "format_mismatch_luminance_luminance_alpha_level_0", "", IVec2(64, 64),
                                                GL_LUMINANCE, GL_LUMINANCE_ALPHA));
    cube->addChild(new IncompleteCubeFormatCase(m_testCtx, m_context.getRenderContext(),
                                                "format_mismatch_luminance_alpha_luminance_level_0", "", IVec2(64, 64),
                                                GL_LUMINANCE_ALPHA, GL_LUMINANCE));
    cube->addChild(new IncompleteCubeFormatCase(m_testCtx, m_context.getRenderContext(),
                                                "format_mismatch_rgb_rgba_level_0_pos_z", "", IVec2(64, 64), GL_RGB,
                                                GL_RGBA, tcu::CUBEFACE_POSITIVE_Z));
    cube->addChild(new IncompleteCubeFormatCase(m_testCtx, m_context.getRenderContext(),
                                                "format_mismatch_rgba_rgb_level_0_neg_z", "", IVec2(64, 64), GL_RGBA,
                                                GL_RGB, tcu::CUBEFACE_NEGATIVE_Z));
    // Cube missing level.
    cube->addChild(new IncompleteCubeMissingLevelCase(m_testCtx, m_context.getRenderContext(), "missing_level_1", "",
                                                      IVec2(64, 64), 1));
    cube->addChild(new IncompleteCubeMissingLevelCase(m_testCtx, m_context.getRenderContext(), "missing_level_3", "",
                                                      IVec2(64, 64), 3));
    cube->addChild(new IncompleteCubeMissingLevelCase(m_testCtx, m_context.getRenderContext(), "missing_level_1_pos_y",
                                                      "", IVec2(64, 64), 1, tcu::CUBEFACE_POSITIVE_Y));
    cube->addChild(new IncompleteCubeMissingLevelCase(m_testCtx, m_context.getRenderContext(), "missing_level_3_neg_y",
                                                      "", IVec2(64, 64), 3, tcu::CUBEFACE_NEGATIVE_Y));
    // Cube wrap modes.
    cube->addChild(new IncompleteCubeWrapModeCase(m_testCtx, m_context.getRenderContext(), "npot_t_repeat", "",
                                                  IVec2(127, 127), GL_CLAMP_TO_EDGE, GL_REPEAT,
                                                  m_context.getContextInfo()));
    cube->addChild(new IncompleteCubeWrapModeCase(m_testCtx, m_context.getRenderContext(), "npot_s_repeat", "",
                                                  IVec2(127, 127), GL_REPEAT, GL_CLAMP_TO_EDGE,
                                                  m_context.getContextInfo()));
    cube->addChild(new IncompleteCubeWrapModeCase(m_testCtx, m_context.getRenderContext(), "npot_all_repeat", "",
                                                  IVec2(127, 127), GL_REPEAT, GL_REPEAT, m_context.getContextInfo()));
    cube->addChild(new IncompleteCubeWrapModeCase(m_testCtx, m_context.getRenderContext(), "npot_mirrored_repeat", "",
                                                  IVec2(127, 127), GL_MIRRORED_REPEAT, GL_MIRRORED_REPEAT,
                                                  m_context.getContextInfo()));
    // Cube extra level.
    cube->addChild(
        new CompleteCubeExtraLevelCase(m_testCtx, m_context.getRenderContext(), "extra_level", "", IVec2(64, 64)));
    // Cube extra level.
    cube->addChild(new IncompleteCubeEmptyObjectCase(m_testCtx, m_context.getRenderContext(), "empty_object", ""));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
