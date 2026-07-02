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
 * \brief Functional rasterization tests.
 *//*--------------------------------------------------------------------*/

#include "es3fRasterizationTests.hpp"
#include "tcuRasterizationVerifier.hpp"
#include "tcuSurface.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuResultCollector.hpp"
#include "gluShaderProgram.hpp"
#include "gluRenderContext.hpp"
#include "gluPixelTransfer.hpp"
#include "gluStrUtil.hpp"
#include "gluTextureUtil.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include <vector>

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace
{

using tcu::LineInterpolationMethod;
using tcu::LineSceneSpec;
using tcu::PointSceneSpec;
using tcu::RasterizationArguments;
using tcu::TriangleSceneSpec;

static const char *const s_shaderVertexTemplate   = "#version 300 es\n"
                                                    "in highp vec4 a_position;\n"
                                                    "in highp vec4 a_color;\n"
                                                    "${INTERPOLATION}out highp vec4 v_color;\n"
                                                    "uniform highp float u_pointSize;\n"
                                                    "void main ()\n"
                                                    "{\n"
                                                    "    gl_Position = a_position;\n"
                                                    "    gl_PointSize = u_pointSize;\n"
                                                    "    v_color = a_color;\n"
                                                    "}\n";
static const char *const s_shaderFragmentTemplate = "#version 300 es\n"
                                                    "layout(location = 0) out highp vec4 fragColor;\n"
                                                    "${INTERPOLATION}in highp vec4 v_color;\n"
                                                    "void main ()\n"
                                                    "{\n"
                                                    "    fragColor = v_color;\n"
                                                    "}\n";
enum InterpolationCaseFlags
{
    INTERPOLATIONFLAGS_NONE      = 0,
    INTERPOLATIONFLAGS_PROJECTED = (1 << 1),
    INTERPOLATIONFLAGS_FLATSHADE = (1 << 2),
};

enum PrimitiveWideness
{
    PRIMITIVEWIDENESS_NARROW = 0,
    PRIMITIVEWIDENESS_WIDE,

    PRIMITIVEWIDENESS_LAST
};

static tcu::PixelFormat getInternalFormatPixelFormat(glw::GLenum internalFormat)
{
    const tcu::IVec4 bitDepth = tcu::getTextureFormatBitDepth(glu::mapGLInternalFormat(internalFormat));
    return tcu::PixelFormat(bitDepth.x(), bitDepth.y(), bitDepth.z(), bitDepth.w());
}

class BaseRenderingCase : public TestCase
{
public:
    enum RenderTarget
    {
        RENDERTARGET_DEFAULT = 0,
        RENDERTARGET_TEXTURE_2D,
        RENDERTARGET_RBO_SINGLESAMPLE,
        RENDERTARGET_RBO_MULTISAMPLE,

        RENDERTARGET_LAST
    };

    enum
    {
        DEFAULT_RENDER_SIZE = 256,
        SAMPLE_COUNT_MAX    = -2,
    };

    BaseRenderingCase(Context &context, const char *name, const char *desc, RenderTarget target, int numSamples,
                      int renderSize);
    ~BaseRenderingCase(void);
    virtual void init(void);
    void deinit(void);

protected:
    void drawPrimitives(tcu::Surface &result, const std::vector<tcu::Vec4> &vertexData, glw::GLenum primitiveType);
    void drawPrimitives(tcu::Surface &result, const std::vector<tcu::Vec4> &vertexData,
                        const std::vector<tcu::Vec4> &coloDrata, glw::GLenum primitiveType);

    virtual float getLineWidth(void) const;
    virtual float getPointSize(void) const;
    const tcu::PixelFormat &getPixelFormat(void) const;

    const int m_renderSize;
    int m_numSamples;
    int m_subpixelBits;
    bool m_flatshade;
    const int m_numRequestedSamples;

private:
    const RenderTarget m_renderTarget;
    const glw::GLenum m_fboInternalFormat;
    const tcu::PixelFormat m_pixelFormat;
    glu::ShaderProgram *m_shader;
    glw::GLuint m_fbo;
    glw::GLuint m_texture;
    glw::GLuint m_rbo;
    glw::GLuint m_blitDstFbo;
    glw::GLuint m_blitDstRbo;
};

BaseRenderingCase::BaseRenderingCase(Context &context, const char *name, const char *desc, RenderTarget target,
                                     int numSamples, int renderSize)
    : TestCase(context, name, desc)
    , m_renderSize(renderSize)
    , m_numSamples(-1)
    , m_subpixelBits(-1)
    , m_flatshade(false)
    , m_numRequestedSamples(numSamples)
    , m_renderTarget(target)
    , m_fboInternalFormat(GL_RGBA8)
    , m_pixelFormat((m_renderTarget == RENDERTARGET_DEFAULT) ? (m_context.getRenderTarget().getPixelFormat()) :
                                                               (getInternalFormatPixelFormat(m_fboInternalFormat)))
    , m_shader(nullptr)
    , m_fbo(0)
    , m_texture(0)
    , m_rbo(0)
    , m_blitDstFbo(0)
    , m_blitDstRbo(0)
{
    DE_ASSERT(m_renderTarget < RENDERTARGET_LAST);
    DE_ASSERT((m_numRequestedSamples == -1) == (m_renderTarget != RENDERTARGET_RBO_MULTISAMPLE));
}

BaseRenderingCase::~BaseRenderingCase(void)
{
    deinit();
}

void BaseRenderingCase::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const int width          = m_context.getRenderTarget().getWidth();
    const int height         = m_context.getRenderTarget().getHeight();
    int msaaTargetSamples    = -1;

    // Requirements

    if (m_renderTarget == RENDERTARGET_DEFAULT && (width < m_renderSize || height < m_renderSize))
        throw tcu::NotSupportedError(std::string("Render target size must be at least ") + de::toString(m_renderSize) +
                                     "x" + de::toString(m_renderSize));

    if (m_renderTarget == RENDERTARGET_RBO_MULTISAMPLE)
    {
        glw::GLint maxSampleCount = 0;
        gl.getInternalformativ(GL_RENDERBUFFER, m_fboInternalFormat, GL_SAMPLES, 1, &maxSampleCount);

        if (m_numRequestedSamples == SAMPLE_COUNT_MAX)
            msaaTargetSamples = maxSampleCount;
        else if (maxSampleCount >= m_numRequestedSamples)
            msaaTargetSamples = m_numRequestedSamples;
        else
            throw tcu::NotSupportedError("Test requires " + de::toString(m_numRequestedSamples) + "x msaa rbo");
    }

    // Gen shader

    {
        tcu::StringTemplate vertexSource(s_shaderVertexTemplate);
        tcu::StringTemplate fragmentSource(s_shaderFragmentTemplate);
        std::map<std::string, std::string> params;

        params["INTERPOLATION"] = (m_flatshade) ? ("flat ") : ("");

        m_shader =
            new glu::ShaderProgram(m_context.getRenderContext(),
                                   glu::ProgramSources() << glu::VertexSource(vertexSource.specialize(params))
                                                         << glu::FragmentSource(fragmentSource.specialize(params)));
        if (!m_shader->isOk())
            throw tcu::TestError("could not create shader");
    }

    // Fbo
    if (m_renderTarget != RENDERTARGET_DEFAULT)
    {
        glw::GLenum error;

        gl.genFramebuffers(1, &m_fbo);
        gl.bindFramebuffer(GL_FRAMEBUFFER, m_fbo);

        switch (m_renderTarget)
        {
        case RENDERTARGET_TEXTURE_2D:
        {
            gl.genTextures(1, &m_texture);
            gl.bindTexture(GL_TEXTURE_2D, m_texture);
            gl.texStorage2D(GL_TEXTURE_2D, 1, m_fboInternalFormat, m_renderSize, m_renderSize);

            error = gl.getError();
            if (error == GL_OUT_OF_MEMORY)
                throw tcu::NotSupportedError("could not create target texture, got out of memory");
            else if (error != GL_NO_ERROR)
                throw tcu::TestError("got " + de::toString(glu::getErrorStr(error)));

            gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
            break;
        }

        case RENDERTARGET_RBO_SINGLESAMPLE:
        case RENDERTARGET_RBO_MULTISAMPLE:
        {
            gl.genRenderbuffers(1, &m_rbo);
            gl.bindRenderbuffer(GL_RENDERBUFFER, m_rbo);

            if (m_renderTarget == RENDERTARGET_RBO_SINGLESAMPLE)
                gl.renderbufferStorage(GL_RENDERBUFFER, m_fboInternalFormat, m_renderSize, m_renderSize);
            else if (m_renderTarget == RENDERTARGET_RBO_MULTISAMPLE)
                gl.renderbufferStorageMultisample(GL_RENDERBUFFER, msaaTargetSamples, m_fboInternalFormat, m_renderSize,
                                                  m_renderSize);
            else
                DE_ASSERT(false);

            error = gl.getError();
            if (error == GL_OUT_OF_MEMORY)
                throw tcu::NotSupportedError("could not create target texture, got out of memory");
            else if (error != GL_NO_ERROR)
                throw tcu::TestError("got " + de::toString(glu::getErrorStr(error)));

            gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_rbo);
            break;
        }

        default:
            DE_ASSERT(false);
        }
    }

    // Resolve (blitFramebuffer) target fbo for MSAA targets
    if (m_renderTarget == RENDERTARGET_RBO_MULTISAMPLE)
    {
        glw::GLenum error;

        gl.genFramebuffers(1, &m_blitDstFbo);
        gl.bindFramebuffer(GL_FRAMEBUFFER, m_blitDstFbo);

        gl.genRenderbuffers(1, &m_blitDstRbo);
        gl.bindRenderbuffer(GL_RENDERBUFFER, m_blitDstRbo);
        gl.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, m_renderSize, m_renderSize);

        error = gl.getError();
        if (error == GL_OUT_OF_MEMORY)
            throw tcu::NotSupportedError("could not create blit target, got out of memory");
        else if (error != GL_NO_ERROR)
            throw tcu::TestError("got " + de::toString(glu::getErrorStr(error)));

        gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_blitDstRbo);

        // restore state
        gl.bindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    }

    // Query info

    if (m_renderTarget == RENDERTARGET_DEFAULT)
        m_numSamples = m_context.getRenderTarget().getNumSamples();
    else if (m_renderTarget == RENDERTARGET_RBO_MULTISAMPLE)
    {
        m_numSamples = -1;
        gl.bindRenderbuffer(GL_RENDERBUFFER, m_rbo);
        gl.getRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &m_numSamples);

        GLU_EXPECT_NO_ERROR(gl.getError(), "get RENDERBUFFER_SAMPLES");
    }
    else
        m_numSamples = 0;

    gl.getIntegerv(GL_SUBPIXEL_BITS, &m_subpixelBits);

    m_testCtx.getLog() << tcu::TestLog::Message << "Sample count = " << m_numSamples << tcu::TestLog::EndMessage;
    m_testCtx.getLog() << tcu::TestLog::Message << "SUBPIXEL_BITS = " << m_subpixelBits << tcu::TestLog::EndMessage;
}

void BaseRenderingCase::deinit(void)
{
    if (m_shader)
    {
        delete m_shader;
        m_shader = nullptr;
    }

    if (m_fbo)
    {
        m_context.getRenderContext().getFunctions().deleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }

    if (m_rbo)
    {
        m_context.getRenderContext().getFunctions().deleteRenderbuffers(1, &m_rbo);
        m_rbo = 0;
    }

    if (m_texture)
    {
        m_context.getRenderContext().getFunctions().deleteTextures(1, &m_texture);
        m_texture = 0;
    }

    if (m_blitDstFbo)
    {
        m_context.getRenderContext().getFunctions().deleteFramebuffers(1, &m_blitDstFbo);
        m_blitDstFbo = 0;
    }

    if (m_blitDstRbo)
    {
        m_context.getRenderContext().getFunctions().deleteRenderbuffers(1, &m_blitDstRbo);
        m_blitDstRbo = 0;
    }
}

void BaseRenderingCase::drawPrimitives(tcu::Surface &result, const std::vector<tcu::Vec4> &vertexData,
                                       glw::GLenum primitiveType)
{
    // default to color white
    const std::vector<tcu::Vec4> colorData(vertexData.size(), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

    drawPrimitives(result, vertexData, colorData, primitiveType);
}

void BaseRenderingCase::drawPrimitives(tcu::Surface &result, const std::vector<tcu::Vec4> &vertexData,
                                       const std::vector<tcu::Vec4> &colorData, glw::GLenum primitiveType)
{
    const glw::Functions &gl      = m_context.getRenderContext().getFunctions();
    const glw::GLint positionLoc  = gl.getAttribLocation(m_shader->getProgram(), "a_position");
    const glw::GLint colorLoc     = gl.getAttribLocation(m_shader->getProgram(), "a_color");
    const glw::GLint pointSizeLoc = gl.getUniformLocation(m_shader->getProgram(), "u_pointSize");

    gl.clearColor(0, 0, 0, 1);
    gl.clear(GL_COLOR_BUFFER_BIT);
    gl.viewport(0, 0, m_renderSize, m_renderSize);
    gl.useProgram(m_shader->getProgram());
    gl.enableVertexAttribArray(positionLoc);
    gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, &vertexData[0]);
    gl.enableVertexAttribArray(colorLoc);
    gl.vertexAttribPointer(colorLoc, 4, GL_FLOAT, GL_FALSE, 0, &colorData[0]);
    gl.uniform1f(pointSizeLoc, getPointSize());
    gl.lineWidth(getLineWidth());
    gl.drawArrays(primitiveType, 0, (glw::GLsizei)vertexData.size());
    gl.disableVertexAttribArray(colorLoc);
    gl.disableVertexAttribArray(positionLoc);
    gl.useProgram(0);
    gl.finish();
    GLU_EXPECT_NO_ERROR(gl.getError(), "draw primitives");

    // read pixels
    if (m_renderTarget == RENDERTARGET_RBO_MULTISAMPLE)
    {
        // resolve msaa
        gl.bindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
        gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, m_blitDstFbo);

        gl.blitFramebuffer(0, 0, m_renderSize, m_renderSize, 0, 0, m_renderSize, m_renderSize, GL_COLOR_BUFFER_BIT,
                           GL_NEAREST);
        GLU_EXPECT_NO_ERROR(gl.getError(), "blit");

        // read resolved
        gl.bindFramebuffer(GL_READ_FRAMEBUFFER, m_blitDstFbo);

        glu::readPixels(m_context.getRenderContext(), 0, 0, result.getAccess());
        GLU_EXPECT_NO_ERROR(gl.getError(), "read pixels");

        gl.bindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    }
    else
    {
        glu::readPixels(m_context.getRenderContext(), 0, 0, result.getAccess());
        GLU_EXPECT_NO_ERROR(gl.getError(), "read pixels");
    }
}

float BaseRenderingCase::getLineWidth(void) const
{
    return 1.0f;
}

float BaseRenderingCase::getPointSize(void) const
{
    return 1.0f;
}

const tcu::PixelFormat &BaseRenderingCase::getPixelFormat(void) const
{
    return m_pixelFormat;
}

class BaseTriangleCase : public BaseRenderingCase
{
public:
    BaseTriangleCase(Context &context, const char *name, const char *desc, glw::GLenum primitiveDrawType,
                     BaseRenderingCase::RenderTarget renderTarget, int numSamples);
    ~BaseTriangleCase(void);
    IterateResult iterate(void);

private:
    virtual void generateTriangles(int iteration, std::vector<tcu::Vec4> &outData,
                                   std::vector<TriangleSceneSpec::SceneTriangle> &outTriangles) = 0;

    int m_iteration;
    const int m_iterationCount;
    const glw::GLenum m_primitiveDrawType;
    bool m_allIterationsPassed;
};

BaseTriangleCase::BaseTriangleCase(Context &context, const char *name, const char *desc, glw::GLenum primitiveDrawType,
                                   BaseRenderingCase::RenderTarget renderTarget, int numSamples)
    : BaseRenderingCase(context, name, desc, renderTarget, numSamples, DEFAULT_RENDER_SIZE)
    , m_iteration(0)
    , m_iterationCount(3)
    , m_primitiveDrawType(primitiveDrawType)
    , m_allIterationsPassed(true)
{
}

BaseTriangleCase::~BaseTriangleCase(void)
{
}

BaseTriangleCase::IterateResult BaseTriangleCase::iterate(void)
{
    const std::string iterationDescription =
        "Test iteration " + de::toString(m_iteration + 1) + " / " + de::toString(m_iterationCount);
    const tcu::ScopedLogSection section(m_testCtx.getLog(), iterationDescription, iterationDescription);
    tcu::Surface resultImage(m_renderSize, m_renderSize);
    std::vector<tcu::Vec4> drawBuffer;
    std::vector<TriangleSceneSpec::SceneTriangle> triangles;

    generateTriangles(m_iteration, drawBuffer, triangles);

    // draw image
    drawPrimitives(resultImage, drawBuffer, m_primitiveDrawType);

    // compare
    {
        bool compareOk;
        RasterizationArguments args;
        TriangleSceneSpec scene;

        args.numSamples   = m_numSamples;
        args.subpixelBits = m_subpixelBits;
        args.redBits      = getPixelFormat().redBits;
        args.greenBits    = getPixelFormat().greenBits;
        args.blueBits     = getPixelFormat().blueBits;

        scene.triangles.swap(triangles);

        compareOk = verifyTriangleGroupRasterization(resultImage, scene, args, m_testCtx.getLog());

        if (!compareOk)
            m_allIterationsPassed = false;
    }

    // result
    if (++m_iteration == m_iterationCount)
    {
        if (m_allIterationsPassed)
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Incorrect rasterization");

        return STOP;
    }
    else
        return CONTINUE;
}

class BaseLineCase : public BaseRenderingCase
{
public:
    BaseLineCase(Context &context, const char *name, const char *desc, glw::GLenum primitiveDrawType,
                 PrimitiveWideness wideness, BaseRenderingCase::RenderTarget renderTarget, int numSamples);
    ~BaseLineCase(void);

    void init(void);
    IterateResult iterate(void);
    float getLineWidth(void) const;

private:
    virtual void generateLines(int iteration, std::vector<tcu::Vec4> &outData,
                               std::vector<LineSceneSpec::SceneLine> &outLines) = 0;

    int m_iteration;
    const int m_iterationCount;
    const glw::GLenum m_primitiveDrawType;
    const PrimitiveWideness m_primitiveWideness;
    bool m_allIterationsPassed;
    bool m_multisampleRelaxationRequired;
    float m_maxLineWidth;
    std::vector<float> m_lineWidths;
};

BaseLineCase::BaseLineCase(Context &context, const char *name, const char *desc, glw::GLenum primitiveDrawType,
                           PrimitiveWideness wideness, BaseRenderingCase::RenderTarget renderTarget, int numSamples)
    : BaseRenderingCase(context, name, desc, renderTarget, numSamples, DEFAULT_RENDER_SIZE)
    , m_iteration(0)
    , m_iterationCount(3)
    , m_primitiveDrawType(primitiveDrawType)
    , m_primitiveWideness(wideness)
    , m_allIterationsPassed(true)
    , m_multisampleRelaxationRequired(false)
    , m_maxLineWidth(1.0f)
{
    DE_ASSERT(m_primitiveWideness < PRIMITIVEWIDENESS_LAST);
}

BaseLineCase::~BaseLineCase(void)
{
}

void BaseLineCase::init(void)
{
    // create line widths
    if (m_primitiveWideness == PRIMITIVEWIDENESS_NARROW)
    {
        m_lineWidths.resize(m_iterationCount, 1.0f);
    }
    else if (m_primitiveWideness == PRIMITIVEWIDENESS_WIDE)
    {
        float range[2] = {0.0f, 0.0f};
        m_context.getRenderContext().getFunctions().getFloatv(GL_ALIASED_LINE_WIDTH_RANGE, range);

        m_testCtx.getLog() << tcu::TestLog::Message << "ALIASED_LINE_WIDTH_RANGE = [" << range[0] << ", " << range[1]
                           << "]" << tcu::TestLog::EndMessage;

        // no wide line support
        if (range[1] <= 1.0f)
            throw tcu::NotSupportedError("wide line support required");

        // set hand picked sizes
        m_lineWidths.push_back(5.0f);
        m_lineWidths.push_back(10.0f);
        m_lineWidths.push_back(range[1]);
        DE_ASSERT((int)m_lineWidths.size() == m_iterationCount);

        m_maxLineWidth = range[1];
    }
    else
        DE_ASSERT(false);

    // init parent
    BaseRenderingCase::init();
}

BaseLineCase::IterateResult BaseLineCase::iterate(void)
{
    const std::string iterationDescription =
        "Test iteration " + de::toString(m_iteration + 1) + " / " + de::toString(m_iterationCount);
    const tcu::ScopedLogSection section(m_testCtx.getLog(), iterationDescription, iterationDescription);
    const float lineWidth = getLineWidth();
    tcu::Surface resultImage(m_renderSize, m_renderSize);
    std::vector<tcu::Vec4> drawBuffer;
    std::vector<LineSceneSpec::SceneLine> lines;

    // supported?
    if (lineWidth <= m_maxLineWidth)
    {
        // gen data
        generateLines(m_iteration, drawBuffer, lines);

        // draw image
        drawPrimitives(resultImage, drawBuffer, m_primitiveDrawType);

        // compare
        {
            bool compareOk;
            RasterizationArguments args;
            LineSceneSpec scene;

            args.numSamples   = m_numSamples;
            args.subpixelBits = m_subpixelBits;
            args.redBits      = getPixelFormat().redBits;
            args.greenBits    = getPixelFormat().greenBits;
            args.blueBits     = getPixelFormat().blueBits;

            scene.lines.swap(lines);
            scene.lineWidth                      = lineWidth;
            scene.stippleFactor                  = 1;
            scene.stipplePattern                 = 0xFFFF;
            scene.allowNonProjectedInterpolation = true;

            compareOk = verifyLineGroupRasterization(resultImage, scene, args, m_testCtx.getLog());

            // multisampled wide lines might not be supported
            if (scene.lineWidth != 1.0f && m_numSamples > 1 && !compareOk)
            {
                m_multisampleRelaxationRequired = true;
                compareOk                       = true;
            }

            if (!compareOk)
                m_allIterationsPassed = false;
        }
    }
    else
        m_testCtx.getLog() << tcu::TestLog::Message << "Line width " << lineWidth
                           << " not supported, skipping iteration." << tcu::TestLog::EndMessage;

    // result
    if (++m_iteration == m_iterationCount)
    {
        if (m_allIterationsPassed && m_multisampleRelaxationRequired)
            m_testCtx.setTestResult(QP_TEST_RESULT_COMPATIBILITY_WARNING,
                                    "Rasterization of multisampled wide lines failed");
        else if (m_allIterationsPassed)
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Incorrect rasterization");

        return STOP;
    }
    else
        return CONTINUE;
}

float BaseLineCase::getLineWidth(void) const
{
    return m_lineWidths[m_iteration];
}

class PointCase : public BaseRenderingCase
{
public:
    PointCase(Context &context, const char *name, const char *desc, PrimitiveWideness wideness,
              BaseRenderingCase::RenderTarget renderTarget = RENDERTARGET_DEFAULT, int numSamples = -1);
    ~PointCase(void);

    void init(void);
    IterateResult iterate(void);

protected:
    float getPointSize(void) const;

private:
    void generatePoints(int iteration, std::vector<tcu::Vec4> &outData,
                        std::vector<PointSceneSpec::ScenePoint> &outPoints);

    int m_iteration;
    const int m_iterationCount;
    const PrimitiveWideness m_primitiveWideness;
    bool m_allIterationsPassed;

    float m_maxPointSize;
    std::vector<float> m_pointSizes;
};

PointCase::PointCase(Context &context, const char *name, const char *desc, PrimitiveWideness wideness,
                     BaseRenderingCase::RenderTarget renderTarget, int numSamples)
    : BaseRenderingCase(context, name, desc, renderTarget, numSamples, DEFAULT_RENDER_SIZE)
    , m_iteration(0)
    , m_iterationCount(3)
    , m_primitiveWideness(wideness)
    , m_allIterationsPassed(true)
    , m_maxPointSize(1.0f)
{
}

PointCase::~PointCase(void)
{
}

void PointCase::init(void)
{
    // create point sizes
    if (m_primitiveWideness == PRIMITIVEWIDENESS_NARROW)
    {
        m_pointSizes.resize(m_iterationCount, 1.0f);
    }
    else if (m_primitiveWideness == PRIMITIVEWIDENESS_WIDE)
    {
        float range[2] = {0.0f, 0.0f};
        m_context.getRenderContext().getFunctions().getFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);

        m_testCtx.getLog() << tcu::TestLog::Message << "GL_ALIASED_POINT_SIZE_RANGE = [" << range[0] << ", " << range[1]
                           << "]" << tcu::TestLog::EndMessage;

        // no wide line support
        if (range[1] <= 1.0f)
            throw tcu::NotSupportedError("wide point support required");

        // set hand picked sizes
        m_pointSizes.push_back(10.0f);
        m_pointSizes.push_back(25.0f);
        m_pointSizes.push_back(range[1]);
        DE_ASSERT((int)m_pointSizes.size() == m_iterationCount);

        m_maxPointSize = range[1];
    }
    else
        DE_ASSERT(false);

    // init parent
    BaseRenderingCase::init();
}

PointCase::IterateResult PointCase::iterate(void)
{
    const std::string iterationDescription =
        "Test iteration " + de::toString(m_iteration + 1) + " / " + de::toString(m_iterationCount);
    const tcu::ScopedLogSection section(m_testCtx.getLog(), iterationDescription, iterationDescription);
    const float pointSize = getPointSize();
    tcu::Surface resultImage(m_renderSize, m_renderSize);
    std::vector<tcu::Vec4> drawBuffer;
    std::vector<PointSceneSpec::ScenePoint> points;

    // supported?
    if (pointSize <= m_maxPointSize)
    {
        // gen data
        generatePoints(m_iteration, drawBuffer, points);

        // draw image
        drawPrimitives(resultImage, drawBuffer, GL_POINTS);

        // compare
        {
            bool compareOk;
            RasterizationArguments args;
            PointSceneSpec scene;

            args.numSamples   = m_numSamples;
            args.subpixelBits = m_subpixelBits;
            args.redBits      = getPixelFormat().redBits;
            args.greenBits    = getPixelFormat().greenBits;
            args.blueBits     = getPixelFormat().blueBits;

            scene.points.swap(points);

            compareOk = verifyPointGroupRasterization(resultImage, scene, args, m_testCtx.getLog());

            if (!compareOk)
                m_allIterationsPassed = false;
        }
    }
    else
        m_testCtx.getLog() << tcu::TestLog::Message << "Point size " << pointSize
                           << " not supported, skipping iteration." << tcu::TestLog::EndMessage;

    // result
    if (++m_iteration == m_iterationCount)
    {
        if (m_allIterationsPassed)
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Incorrect rasterization");

        return STOP;
    }
    else
        return CONTINUE;
}

float PointCase::getPointSize(void) const
{
    return m_pointSizes[m_iteration];
}

void PointCase::generatePoints(int iteration, std::vector<tcu::Vec4> &outData,
                               std::vector<PointSceneSpec::ScenePoint> &outPoints)
{
    outData.resize(6);

    switch (iteration)
    {
    case 0:
        // \note: these values are chosen arbitrarily
        outData[0] = tcu::Vec4(0.2f, 0.8f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(0.5f, 0.2f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.5f, 0.3f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(-0.5f, 0.2f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(-0.2f, -0.4f, 0.0f, 1.0f);
        outData[5] = tcu::Vec4(-0.4f, 0.2f, 0.0f, 1.0f);
        break;

    case 1:
        outData[0] = tcu::Vec4(-0.499f, 0.128f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(-0.501f, -0.3f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.11f, -0.2f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(0.11f, 0.2f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(0.88f, 0.9f, 0.0f, 1.0f);
        outData[5] = tcu::Vec4(0.4f, 1.2f, 0.0f, 1.0f);
        break;

    case 2:
        outData[0] = tcu::Vec4(-0.9f, -0.3f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(0.3f, -0.9f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(-0.4f, -0.1f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(-0.11f, 0.2f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(0.88f, 0.7f, 0.0f, 1.0f);
        outData[5] = tcu::Vec4(-0.4f, 0.4f, 0.0f, 1.0f);
        break;
    }

    outPoints.resize(outData.size());
    for (int pointNdx = 0; pointNdx < (int)outPoints.size(); ++pointNdx)
    {
        outPoints[pointNdx].position  = outData[pointNdx];
        outPoints[pointNdx].pointSize = getPointSize();
    }

    // log
    m_testCtx.getLog() << tcu::TestLog::Message << "Rendering " << outPoints.size()
                       << " point(s): (point size = " << getPointSize() << ")" << tcu::TestLog::EndMessage;
    for (int pointNdx = 0; pointNdx < (int)outPoints.size(); ++pointNdx)
        m_testCtx.getLog() << tcu::TestLog::Message << "Point " << (pointNdx + 1) << ":\t"
                           << outPoints[pointNdx].position << tcu::TestLog::EndMessage;
}

class TrianglesCase : public BaseTriangleCase
{
public:
    TrianglesCase(Context &context, const char *name, const char *desc,
                  BaseRenderingCase::RenderTarget renderTarget = RENDERTARGET_DEFAULT, int numSamples = -1);
    ~TrianglesCase(void);

    void generateTriangles(int iteration, std::vector<tcu::Vec4> &outData,
                           std::vector<TriangleSceneSpec::SceneTriangle> &outTriangles);
};

TrianglesCase::TrianglesCase(Context &context, const char *name, const char *desc,
                             BaseRenderingCase::RenderTarget renderTarget, int numSamples)
    : BaseTriangleCase(context, name, desc, GL_TRIANGLES, renderTarget, numSamples)
{
}

TrianglesCase::~TrianglesCase(void)
{
}

void TrianglesCase::generateTriangles(int iteration, std::vector<tcu::Vec4> &outData,
                                      std::vector<TriangleSceneSpec::SceneTriangle> &outTriangles)
{
    outData.resize(6);

    switch (iteration)
    {
    case 0:
        // \note: these values are chosen arbitrarily
        outData[0] = tcu::Vec4(0.2f, 0.8f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(0.5f, 0.2f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.5f, 0.3f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(-0.5f, 0.2f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(-1.5f, -0.4f, 0.0f, 1.0f);
        outData[5] = tcu::Vec4(-0.4f, 0.2f, 0.0f, 1.0f);
        break;

    case 1:
        outData[0] = tcu::Vec4(-0.499f, 0.128f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(-0.501f, -0.3f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.11f, -0.2f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(0.11f, 0.2f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(0.88f, 0.9f, 0.0f, 1.0f);
        outData[5] = tcu::Vec4(0.4f, 1.2f, 0.0f, 1.0f);
        break;

    case 2:
        outData[0] = tcu::Vec4(-0.9f, -0.3f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(1.1f, -0.9f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(-1.1f, -0.1f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(-0.11f, 0.2f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(0.88f, 0.7f, 0.0f, 1.0f);
        outData[5] = tcu::Vec4(-0.4f, 0.4f, 0.0f, 1.0f);
        break;
    }

    outTriangles.resize(2);
    outTriangles[0].positions[0]  = outData[0];
    outTriangles[0].sharedEdge[0] = false;
    outTriangles[0].positions[1]  = outData[1];
    outTriangles[0].sharedEdge[1] = false;
    outTriangles[0].positions[2]  = outData[2];
    outTriangles[0].sharedEdge[2] = false;

    outTriangles[1].positions[0]  = outData[3];
    outTriangles[1].sharedEdge[0] = false;
    outTriangles[1].positions[1]  = outData[4];
    outTriangles[1].sharedEdge[1] = false;
    outTriangles[1].positions[2]  = outData[5];
    outTriangles[1].sharedEdge[2] = false;

    // log
    m_testCtx.getLog() << tcu::TestLog::Message << "Rendering " << outTriangles.size()
                       << " triangle(s):" << tcu::TestLog::EndMessage;
    for (int triangleNdx = 0; triangleNdx < (int)outTriangles.size(); ++triangleNdx)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Triangle " << (triangleNdx + 1) << ":"
                           << "\n\t" << outTriangles[triangleNdx].positions[0] << "\n\t"
                           << outTriangles[triangleNdx].positions[1] << "\n\t" << outTriangles[triangleNdx].positions[2]
                           << tcu::TestLog::EndMessage;
    }
}

class TriangleStripCase : public BaseTriangleCase
{
public:
    TriangleStripCase(Context &context, const char *name, const char *desc,
                      BaseRenderingCase::RenderTarget renderTarget = RENDERTARGET_DEFAULT, int numSamples = -1);

    void generateTriangles(int iteration, std::vector<tcu::Vec4> &outData,
                           std::vector<TriangleSceneSpec::SceneTriangle> &outTriangles);
};

TriangleStripCase::TriangleStripCase(Context &context, const char *name, const char *desc,
                                     BaseRenderingCase::RenderTarget renderTarget, int numSamples)
    : BaseTriangleCase(context, name, desc, GL_TRIANGLE_STRIP, renderTarget, numSamples)
{
}

void TriangleStripCase::generateTriangles(int iteration, std::vector<tcu::Vec4> &outData,
                                          std::vector<TriangleSceneSpec::SceneTriangle> &outTriangles)
{
    outData.resize(5);

    switch (iteration)
    {
    case 0:
        // \note: these values are chosen arbitrarily
        outData[0] = tcu::Vec4(-0.504f, 0.8f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(-0.2f, -0.2f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(-0.2f, 0.199f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(0.5f, 0.201f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(1.5f, 0.4f, 0.0f, 1.0f);
        break;

    case 1:
        outData[0] = tcu::Vec4(-0.499f, 0.129f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(-0.501f, -0.3f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.11f, -0.2f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(0.11f, -0.31f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(0.88f, 0.9f, 0.0f, 1.0f);
        break;

    case 2:
        outData[0] = tcu::Vec4(-0.9f, -0.3f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(1.1f, -0.9f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(-0.87f, -0.1f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(-0.11f, 0.19f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(0.88f, 0.7f, 0.0f, 1.0f);
        break;
    }

    outTriangles.resize(3);
    outTriangles[0].positions[0]  = outData[0];
    outTriangles[0].sharedEdge[0] = false;
    outTriangles[0].positions[1]  = outData[1];
    outTriangles[0].sharedEdge[1] = true;
    outTriangles[0].positions[2]  = outData[2];
    outTriangles[0].sharedEdge[2] = false;

    outTriangles[1].positions[0]  = outData[2];
    outTriangles[1].sharedEdge[0] = true;
    outTriangles[1].positions[1]  = outData[1];
    outTriangles[1].sharedEdge[1] = false;
    outTriangles[1].positions[2]  = outData[3];
    outTriangles[1].sharedEdge[2] = true;

    outTriangles[2].positions[0]  = outData[2];
    outTriangles[2].sharedEdge[0] = true;
    outTriangles[2].positions[1]  = outData[3];
    outTriangles[2].sharedEdge[1] = false;
    outTriangles[2].positions[2]  = outData[4];
    outTriangles[2].sharedEdge[2] = false;

    // log
    m_testCtx.getLog() << tcu::TestLog::Message << "Rendering triangle strip, " << outData.size() << " vertices."
                       << tcu::TestLog::EndMessage;
    for (int vtxNdx = 0; vtxNdx < (int)outData.size(); ++vtxNdx)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "\t" << outData[vtxNdx] << tcu::TestLog::EndMessage;
    }
}

class TriangleFanCase : public BaseTriangleCase
{
public:
    TriangleFanCase(Context &context, const char *name, const char *desc,
                    BaseRenderingCase::RenderTarget renderTarget = RENDERTARGET_DEFAULT, int numSamples = -1);

    void generateTriangles(int iteration, std::vector<tcu::Vec4> &outData,
                           std::vector<TriangleSceneSpec::SceneTriangle> &outTriangles);
};

TriangleFanCase::TriangleFanCase(Context &context, const char *name, const char *desc,
                                 BaseRenderingCase::RenderTarget renderTarget, int numSamples)
    : BaseTriangleCase(context, name, desc, GL_TRIANGLE_FAN, renderTarget, numSamples)
{
}

void TriangleFanCase::generateTriangles(int iteration, std::vector<tcu::Vec4> &outData,
                                        std::vector<TriangleSceneSpec::SceneTriangle> &outTriangles)
{
    outData.resize(5);

    switch (iteration)
    {
    case 0:
        // \note: these values are chosen arbitrarily
        outData[0] = tcu::Vec4(0.01f, 0.0f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(0.5f, 0.2f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.46f, 0.3f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(-0.5f, 0.2f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(-1.5f, -0.4f, 0.0f, 1.0f);
        break;

    case 1:
        outData[0] = tcu::Vec4(-0.499f, 0.128f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(-0.501f, -0.3f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.11f, -0.2f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(0.11f, 0.2f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(0.88f, 0.9f, 0.0f, 1.0f);
        break;

    case 2:
        outData[0] = tcu::Vec4(-0.9f, -0.3f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(1.1f, -0.9f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.7f, -0.1f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(0.11f, 0.2f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(0.88f, 0.7f, 0.0f, 1.0f);
        break;
    }

    outTriangles.resize(3);
    outTriangles[0].positions[0]  = outData[0];
    outTriangles[0].sharedEdge[0] = false;
    outTriangles[0].positions[1]  = outData[1];
    outTriangles[0].sharedEdge[1] = false;
    outTriangles[0].positions[2]  = outData[2];
    outTriangles[0].sharedEdge[2] = true;

    outTriangles[1].positions[0]  = outData[0];
    outTriangles[1].sharedEdge[0] = true;
    outTriangles[1].positions[1]  = outData[2];
    outTriangles[1].sharedEdge[1] = false;
    outTriangles[1].positions[2]  = outData[3];
    outTriangles[1].sharedEdge[2] = true;

    outTriangles[2].positions[0]  = outData[0];
    outTriangles[2].sharedEdge[0] = true;
    outTriangles[2].positions[1]  = outData[3];
    outTriangles[2].sharedEdge[1] = false;
    outTriangles[2].positions[2]  = outData[4];
    outTriangles[2].sharedEdge[2] = false;

    // log
    m_testCtx.getLog() << tcu::TestLog::Message << "Rendering triangle fan, " << outData.size() << " vertices."
                       << tcu::TestLog::EndMessage;
    for (int vtxNdx = 0; vtxNdx < (int)outData.size(); ++vtxNdx)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "\t" << outData[vtxNdx] << tcu::TestLog::EndMessage;
    }
}

class LinesCase : public BaseLineCase
{
public:
    LinesCase(Context &context, const char *name, const char *desc, PrimitiveWideness wideness,
              BaseRenderingCase::RenderTarget renderTarget = RENDERTARGET_DEFAULT, int numSamples = -1);

    void generateLines(int iteration, std::vector<tcu::Vec4> &outData, std::vector<LineSceneSpec::SceneLine> &outLines);
};

LinesCase::LinesCase(Context &context, const char *name, const char *desc, PrimitiveWideness wideness,
                     BaseRenderingCase::RenderTarget renderTarget, int numSamples)
    : BaseLineCase(context, name, desc, GL_LINES, wideness, renderTarget, numSamples)
{
}

void LinesCase::generateLines(int iteration, std::vector<tcu::Vec4> &outData,
                              std::vector<LineSceneSpec::SceneLine> &outLines)
{
    outData.resize(6);

    switch (iteration)
    {
    case 0:
        // \note: these values are chosen arbitrarily
        outData[0] = tcu::Vec4(0.01f, 0.0f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(0.5f, 0.2f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.46f, 0.3f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(-0.3f, 0.2f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(-1.5f, -0.4f, 0.0f, 1.0f);
        outData[5] = tcu::Vec4(0.1f, 0.5f, 0.0f, 1.0f);
        break;

    case 1:
        outData[0] = tcu::Vec4(-0.499f, 0.128f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(-0.501f, -0.3f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.11f, -0.2f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(0.11f, 0.2f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(0.88f, 0.9f, 0.0f, 1.0f);
        outData[5] = tcu::Vec4(0.18f, -0.2f, 0.0f, 1.0f);
        break;

    case 2:
        outData[0] = tcu::Vec4(-0.9f, -0.3f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(1.1f, -0.9f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.7f, -0.1f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(0.11f, 0.2f, 0.0f, 1.0f);
        outData[4] = tcu::Vec4(0.88f, 0.7f, 0.0f, 1.0f);
        outData[5] = tcu::Vec4(0.8f, -0.7f, 0.0f, 1.0f);
        break;
    }

    outLines.resize(3);
    outLines[0].positions[0] = outData[0];
    outLines[0].positions[1] = outData[1];
    outLines[1].positions[0] = outData[2];
    outLines[1].positions[1] = outData[3];
    outLines[2].positions[0] = outData[4];
    outLines[2].positions[1] = outData[5];

    // log
    m_testCtx.getLog() << tcu::TestLog::Message << "Rendering " << outLines.size()
                       << " lines(s): (width = " << getLineWidth() << ")" << tcu::TestLog::EndMessage;
    for (int lineNdx = 0; lineNdx < (int)outLines.size(); ++lineNdx)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Line " << (lineNdx + 1) << ":"
                           << "\n\t" << outLines[lineNdx].positions[0] << "\n\t" << outLines[lineNdx].positions[1]
                           << tcu::TestLog::EndMessage;
    }
}

class LineStripCase : public BaseLineCase
{
public:
    LineStripCase(Context &context, const char *name, const char *desc, PrimitiveWideness wideness,
                  BaseRenderingCase::RenderTarget renderTarget = RENDERTARGET_DEFAULT, int numSamples = -1);

    void generateLines(int iteration, std::vector<tcu::Vec4> &outData, std::vector<LineSceneSpec::SceneLine> &outLines);
};

LineStripCase::LineStripCase(Context &context, const char *name, const char *desc, PrimitiveWideness wideness,
                             BaseRenderingCase::RenderTarget renderTarget, int numSamples)
    : BaseLineCase(context, name, desc, GL_LINE_STRIP, wideness, renderTarget, numSamples)
{
}

void LineStripCase::generateLines(int iteration, std::vector<tcu::Vec4> &outData,
                                  std::vector<LineSceneSpec::SceneLine> &outLines)
{
    outData.resize(4);

    switch (iteration)
    {
    case 0:
        // \note: these values are chosen arbitrarily
        outData[0] = tcu::Vec4(0.01f, 0.0f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(0.5f, 0.2f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.46f, 0.3f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(-0.5f, 0.2f, 0.0f, 1.0f);
        break;

    case 1:
        outData[0] = tcu::Vec4(-0.499f, 0.128f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(-0.501f, -0.3f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.11f, -0.2f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(0.11f, 0.2f, 0.0f, 1.0f);
        break;

    case 2:
        outData[0] = tcu::Vec4(-0.9f, -0.3f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(1.1f, -0.9f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.7f, -0.1f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(0.11f, 0.2f, 0.0f, 1.0f);
        break;
    }

    outLines.resize(3);
    outLines[0].positions[0] = outData[0];
    outLines[0].positions[1] = outData[1];
    outLines[1].positions[0] = outData[1];
    outLines[1].positions[1] = outData[2];
    outLines[2].positions[0] = outData[2];
    outLines[2].positions[1] = outData[3];

    // log
    m_testCtx.getLog() << tcu::TestLog::Message << "Rendering line strip, width = " << getLineWidth() << ", "
                       << outData.size() << " vertices." << tcu::TestLog::EndMessage;
    for (int vtxNdx = 0; vtxNdx < (int)outData.size(); ++vtxNdx)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "\t" << outData[vtxNdx] << tcu::TestLog::EndMessage;
    }
}

class LineLoopCase : public BaseLineCase
{
public:
    LineLoopCase(Context &context, const char *name, const char *desc, PrimitiveWideness wideness,
                 BaseRenderingCase::RenderTarget renderTarget = RENDERTARGET_DEFAULT, int numSamples = -1);

    void generateLines(int iteration, std::vector<tcu::Vec4> &outData, std::vector<LineSceneSpec::SceneLine> &outLines);
};

LineLoopCase::LineLoopCase(Context &context, const char *name, const char *desc, PrimitiveWideness wideness,
                           BaseRenderingCase::RenderTarget renderTarget, int numSamples)
    : BaseLineCase(context, name, desc, GL_LINE_LOOP, wideness, renderTarget, numSamples)
{
}

void LineLoopCase::generateLines(int iteration, std::vector<tcu::Vec4> &outData,
                                 std::vector<LineSceneSpec::SceneLine> &outLines)
{
    outData.resize(4);

    switch (iteration)
    {
    case 0:
        // \note: these values are chosen arbitrarily
        outData[0] = tcu::Vec4(0.01f, 0.0f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(0.5f, 0.2f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.46f, 0.3f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(-0.5f, 0.2f, 0.0f, 1.0f);
        break;

    case 1:
        outData[0] = tcu::Vec4(-0.499f, 0.128f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(-0.501f, -0.3f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.11f, -0.2f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(0.11f, 0.2f, 0.0f, 1.0f);
        break;

    case 2:
        outData[0] = tcu::Vec4(-0.9f, -0.3f, 0.0f, 1.0f);
        outData[1] = tcu::Vec4(1.1f, -0.9f, 0.0f, 1.0f);
        outData[2] = tcu::Vec4(0.7f, -0.1f, 0.0f, 1.0f);
        outData[3] = tcu::Vec4(0.11f, 0.2f, 0.0f, 1.0f);
        break;
    }

    outLines.resize(4);
    outLines[0].positions[0] = outData[0];
    outLines[0].positions[1] = outData[1];
    outLines[1].positions[0] = outData[1];
    outLines[1].positions[1] = outData[2];
    outLines[2].positions[0] = outData[2];
    outLines[2].positions[1] = outData[3];
    outLines[3].positions[0] = outData[3];
    outLines[3].positions[1] = outData[0];

    // log
    m_testCtx.getLog() << tcu::TestLog::Message << "Rendering line loop, width = " << getLineWidth() << ", "
                       << outData.size() << " vertices." << tcu::TestLog::EndMessage;
    for (int vtxNdx = 0; vtxNdx < (int)outData.size(); ++vtxNdx)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "\t" << outData[vtxNdx] << tcu::TestLog::EndMessage;
    }
}

class FillRuleCase : public BaseRenderingCase
{
public:
    enum FillRuleCaseType
    {
        FILLRULECASE_BASIC = 0,
        FILLRULECASE_REVERSED,
        FILLRULECASE_CLIPPED_FULL,
        FILLRULECASE_CLIPPED_PARTIAL,
        FILLRULECASE_PROJECTED,

        FILLRULECASE_LAST
    };

    FillRuleCase(Context &ctx, const char *name, const char *desc, FillRuleCaseType type,
                 RenderTarget renderTarget = RENDERTARGET_DEFAULT, int numSamples = -1);
    ~FillRuleCase(void);
    IterateResult iterate(void);

private:
    static int getRenderSize(FillRuleCase::FillRuleCaseType type);
    static int getNumIterations(FillRuleCase::FillRuleCaseType type);
    void generateTriangles(int iteration, std::vector<tcu::Vec4> &outData) const;

    const FillRuleCaseType m_caseType;
    int m_iteration;
    const int m_iterationCount;
    bool m_allIterationsPassed;
};

FillRuleCase::FillRuleCase(Context &ctx, const char *name, const char *desc, FillRuleCaseType type,
                           RenderTarget renderTarget, int numSamples)
    : BaseRenderingCase(ctx, name, desc, renderTarget, numSamples, getRenderSize(type))
    , m_caseType(type)
    , m_iteration(0)
    , m_iterationCount(getNumIterations(type))
    , m_allIterationsPassed(true)
{
    DE_ASSERT(type < FILLRULECASE_LAST);
}

FillRuleCase::~FillRuleCase(void)
{
    deinit();
}

FillRuleCase::IterateResult FillRuleCase::iterate(void)
{
    const std::string iterationDescription =
        "Test iteration " + de::toString(m_iteration + 1) + " / " + de::toString(m_iterationCount);
    const tcu::ScopedLogSection section(m_testCtx.getLog(), iterationDescription, iterationDescription);
    const int thresholdRed   = 1 << (8 - getPixelFormat().redBits);
    const int thresholdGreen = 1 << (8 - getPixelFormat().greenBits);
    const int thresholdBlue  = 1 << (8 - getPixelFormat().blueBits);
    tcu::Surface resultImage(m_renderSize, m_renderSize);
    std::vector<tcu::Vec4> drawBuffer;
    bool imageShown = false;

    generateTriangles(m_iteration, drawBuffer);

    // draw image
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const std::vector<tcu::Vec4> colorBuffer(drawBuffer.size(), tcu::Vec4(0.5f, 0.5f, 0.5f, 1.0f));

        m_testCtx.getLog()
            << tcu::TestLog::Message
            << "Drawing gray triangles with shared edges.\nEnabling additive blending to detect overlapping fragments."
            << tcu::TestLog::EndMessage;

        gl.enable(GL_BLEND);
        gl.blendEquation(GL_FUNC_ADD);
        gl.blendFunc(GL_ONE, GL_ONE);
        drawPrimitives(resultImage, drawBuffer, colorBuffer, GL_TRIANGLES);
    }

    // verify no overdraw
    {
        const tcu::RGBA triangleColor = tcu::RGBA(127, 127, 127, 255);
        bool overdraw                 = false;

        m_testCtx.getLog() << tcu::TestLog::Message << "Verifying result." << tcu::TestLog::EndMessage;

        for (int y = 0; y < resultImage.getHeight(); ++y)
            for (int x = 0; x < resultImage.getWidth(); ++x)
            {
                const tcu::RGBA color = resultImage.getPixel(x, y);

                // color values are greater than triangle color? Allow lower values for multisampled edges and background.
                if ((color.getRed() - triangleColor.getRed()) > thresholdRed ||
                    (color.getGreen() - triangleColor.getGreen()) > thresholdGreen ||
                    (color.getBlue() - triangleColor.getBlue()) > thresholdBlue)
                    overdraw = true;
            }

        // results
        if (!overdraw)
            m_testCtx.getLog() << tcu::TestLog::Message << "No overlapping fragments detected."
                               << tcu::TestLog::EndMessage;
        else
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Overlapping fragments detected, image is not valid."
                               << tcu::TestLog::EndMessage;
            m_testCtx.getLog() << tcu::TestLog::ImageSet("Result of rendering", "Result of rendering")
                               << tcu::TestLog::Image("Result", "Result", resultImage) << tcu::TestLog::EndImageSet;

            imageShown            = true;
            m_allIterationsPassed = false;
        }
    }

    // verify no missing fragments in the full viewport case
    if (m_caseType == FILLRULECASE_CLIPPED_FULL)
    {
        bool missingFragments = false;

        m_testCtx.getLog() << tcu::TestLog::Message << "Searching missing fragments." << tcu::TestLog::EndMessage;

        for (int y = 0; y < resultImage.getHeight(); ++y)
            for (int x = 0; x < resultImage.getWidth(); ++x)
            {
                const tcu::RGBA color = resultImage.getPixel(x, y);

                // black? (background)
                if (color.getRed() <= thresholdRed || color.getGreen() <= thresholdGreen ||
                    color.getBlue() <= thresholdBlue)
                    missingFragments = true;
            }

        // results
        if (!missingFragments)
            m_testCtx.getLog() << tcu::TestLog::Message << "No missing fragments detected." << tcu::TestLog::EndMessage;
        else
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Missing fragments detected, image is not valid."
                               << tcu::TestLog::EndMessage;

            if (!imageShown)
            {
                m_testCtx.getLog() << tcu::TestLog::ImageSet("Result of rendering", "Result of rendering")
                                   << tcu::TestLog::Image("Result", "Result", resultImage) << tcu::TestLog::EndImageSet;
            }

            m_allIterationsPassed = false;
        }
    }

    // result
    if (++m_iteration == m_iterationCount)
    {
        if (m_allIterationsPassed)
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Found invalid pixels");

        return STOP;
    }
    else
        return CONTINUE;
}

int FillRuleCase::getRenderSize(FillRuleCase::FillRuleCaseType type)
{
    if (type == FILLRULECASE_CLIPPED_FULL || type == FILLRULECASE_CLIPPED_PARTIAL)
        return DEFAULT_RENDER_SIZE / 4;
    else
        return DEFAULT_RENDER_SIZE;
}

int FillRuleCase::getNumIterations(FillRuleCase::FillRuleCaseType type)
{
    if (type == FILLRULECASE_CLIPPED_FULL || type == FILLRULECASE_CLIPPED_PARTIAL)
        return 15;
    else
        return 2;
}

void FillRuleCase::generateTriangles(int iteration, std::vector<tcu::Vec4> &outData) const
{
    switch (m_caseType)
    {
    case FILLRULECASE_BASIC:
    case FILLRULECASE_REVERSED:
    case FILLRULECASE_PROJECTED:
    {
        const int numRows    = 4;
        const int numColumns = 4;
        const float quadSide = 0.15f;
        de::Random rnd(0xabcd);

        outData.resize(6 * numRows * numColumns);

        for (int col = 0; col < numColumns; ++col)
            for (int row = 0; row < numRows; ++row)
            {
                const tcu::Vec2 center = tcu::Vec2(((float)row + 0.5f) / (float)numRows * 2.0f - 1.0f,
                                                   ((float)col + 0.5f) / (float)numColumns * 2.0f - 1.0f);
                const float rotation   = (float)(iteration * numColumns * numRows + col * numRows + row) /
                                       (float)(m_iterationCount * numColumns * numRows) * DE_PI / 2.0f;
                const tcu::Vec2 sideH   = quadSide * tcu::Vec2(deFloatCos(rotation), deFloatSin(rotation));
                const tcu::Vec2 sideV   = tcu::Vec2(sideH.y(), -sideH.x());
                const tcu::Vec2 quad[4] = {
                    center + sideH + sideV,
                    center + sideH - sideV,
                    center - sideH - sideV,
                    center - sideH + sideV,
                };

                if (m_caseType == FILLRULECASE_BASIC)
                {
                    outData[6 * (col * numRows + row) + 0] = tcu::Vec4(quad[0].x(), quad[0].y(), 0.0f, 1.0f);
                    outData[6 * (col * numRows + row) + 1] = tcu::Vec4(quad[1].x(), quad[1].y(), 0.0f, 1.0f);
                    outData[6 * (col * numRows + row) + 2] = tcu::Vec4(quad[2].x(), quad[2].y(), 0.0f, 1.0f);
                    outData[6 * (col * numRows + row) + 3] = tcu::Vec4(quad[2].x(), quad[2].y(), 0.0f, 1.0f);
                    outData[6 * (col * numRows + row) + 4] = tcu::Vec4(quad[0].x(), quad[0].y(), 0.0f, 1.0f);
                    outData[6 * (col * numRows + row) + 5] = tcu::Vec4(quad[3].x(), quad[3].y(), 0.0f, 1.0f);
                }
                else if (m_caseType == FILLRULECASE_REVERSED)
                {
                    outData[6 * (col * numRows + row) + 0] = tcu::Vec4(quad[0].x(), quad[0].y(), 0.0f, 1.0f);
                    outData[6 * (col * numRows + row) + 1] = tcu::Vec4(quad[1].x(), quad[1].y(), 0.0f, 1.0f);
                    outData[6 * (col * numRows + row) + 2] = tcu::Vec4(quad[2].x(), quad[2].y(), 0.0f, 1.0f);
                    outData[6 * (col * numRows + row) + 3] = tcu::Vec4(quad[0].x(), quad[0].y(), 0.0f, 1.0f);
                    outData[6 * (col * numRows + row) + 4] = tcu::Vec4(quad[2].x(), quad[2].y(), 0.0f, 1.0f);
                    outData[6 * (col * numRows + row) + 5] = tcu::Vec4(quad[3].x(), quad[3].y(), 0.0f, 1.0f);
                }
                else if (m_caseType == FILLRULECASE_PROJECTED)
                {
                    const float w0 = rnd.getFloat(0.1f, 4.0f);
                    const float w1 = rnd.getFloat(0.1f, 4.0f);
                    const float w2 = rnd.getFloat(0.1f, 4.0f);
                    const float w3 = rnd.getFloat(0.1f, 4.0f);

                    outData[6 * (col * numRows + row) + 0] = tcu::Vec4(quad[0].x() * w0, quad[0].y() * w0, 0.0f, w0);
                    outData[6 * (col * numRows + row) + 1] = tcu::Vec4(quad[1].x() * w1, quad[1].y() * w1, 0.0f, w1);
                    outData[6 * (col * numRows + row) + 2] = tcu::Vec4(quad[2].x() * w2, quad[2].y() * w2, 0.0f, w2);
                    outData[6 * (col * numRows + row) + 3] = tcu::Vec4(quad[2].x() * w2, quad[2].y() * w2, 0.0f, w2);
                    outData[6 * (col * numRows + row) + 4] = tcu::Vec4(quad[0].x() * w0, quad[0].y() * w0, 0.0f, w0);
                    outData[6 * (col * numRows + row) + 5] = tcu::Vec4(quad[3].x() * w3, quad[3].y() * w3, 0.0f, w3);
                }
                else
                    DE_ASSERT(false);
            }

        break;
    }

    case FILLRULECASE_CLIPPED_PARTIAL:
    case FILLRULECASE_CLIPPED_FULL:
    {
        const float quadSide = (m_caseType == FILLRULECASE_CLIPPED_PARTIAL) ? (1.0f) : (2.0f);
        const tcu::Vec2 center =
            (m_caseType == FILLRULECASE_CLIPPED_PARTIAL) ? (tcu::Vec2(0.5f, 0.5f)) : (tcu::Vec2(0.0f, 0.0f));
        const float rotation    = (float)(iteration) / (float)(m_iterationCount - 1) * DE_PI / 2.0f;
        const tcu::Vec2 sideH   = quadSide * tcu::Vec2(deFloatCos(rotation), deFloatSin(rotation));
        const tcu::Vec2 sideV   = tcu::Vec2(sideH.y(), -sideH.x());
        const tcu::Vec2 quad[4] = {
            center + sideH + sideV,
            center + sideH - sideV,
            center - sideH - sideV,
            center - sideH + sideV,
        };

        outData.resize(6);
        outData[0] = tcu::Vec4(quad[0].x(), quad[0].y(), 0.0f, 1.0f);
        outData[1] = tcu::Vec4(quad[1].x(), quad[1].y(), 0.0f, 1.0f);
        outData[2] = tcu::Vec4(quad[2].x(), quad[2].y(), 0.0f, 1.0f);
        outData[3] = tcu::Vec4(quad[2].x(), quad[2].y(), 0.0f, 1.0f);
        outData[4] = tcu::Vec4(quad[0].x(), quad[0].y(), 0.0f, 1.0f);
        outData[5] = tcu::Vec4(quad[3].x(), quad[3].y(), 0.0f, 1.0f);
        break;
    }

    default:
        DE_ASSERT(false);
    }
}

class CullingTest : public BaseRenderingCase
{
public:
    CullingTest(Context &ctx, const char *name, const char *desc, glw::GLenum cullMode, glw::GLenum primitive,
                glw::GLenum faceOrder);
    ~CullingTest(void);
    IterateResult iterate(void);

private:
    void generateVertices(std::vector<tcu::Vec4> &outData) const;
    void extractTriangles(std::vector<TriangleSceneSpec::SceneTriangle> &outTriangles,
                          const std::vector<tcu::Vec4> &vertices) const;
    bool triangleOrder(const tcu::Vec4 &v0, const tcu::Vec4 &v1, const tcu::Vec4 &v2) const;

    const glw::GLenum m_cullMode;
    const glw::GLenum m_primitive;
    const glw::GLenum m_faceOrder;
};

CullingTest::CullingTest(Context &ctx, const char *name, const char *desc, glw::GLenum cullMode, glw::GLenum primitive,
                         glw::GLenum faceOrder)
    : BaseRenderingCase(ctx, name, desc, RENDERTARGET_DEFAULT, -1, DEFAULT_RENDER_SIZE)
    , m_cullMode(cullMode)
    , m_primitive(primitive)
    , m_faceOrder(faceOrder)
{
}

CullingTest::~CullingTest(void)
{
}

CullingTest::IterateResult CullingTest::iterate(void)
{
    tcu::Surface resultImage(m_renderSize, m_renderSize);
    std::vector<tcu::Vec4> drawBuffer;
    std::vector<TriangleSceneSpec::SceneTriangle> triangles;

    // generate scene
    generateVertices(drawBuffer);
    extractTriangles(triangles, drawBuffer);

    // draw image
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();

        gl.enable(GL_CULL_FACE);
        gl.cullFace(m_cullMode);
        gl.frontFace(m_faceOrder);

        m_testCtx.getLog() << tcu::TestLog::Message << "Setting front face to " << glu::getWindingName(m_faceOrder)
                           << tcu::TestLog::EndMessage;
        m_testCtx.getLog() << tcu::TestLog::Message << "Setting cull face to " << glu::getFaceName(m_cullMode)
                           << tcu::TestLog::EndMessage;
        m_testCtx.getLog() << tcu::TestLog::Message << "Drawing test pattern ("
                           << glu::getPrimitiveTypeName(m_primitive) << ")" << tcu::TestLog::EndMessage;

        drawPrimitives(resultImage, drawBuffer, m_primitive);
    }

    // compare
    {
        RasterizationArguments args;
        TriangleSceneSpec scene;

        args.numSamples   = m_numSamples;
        args.subpixelBits = m_subpixelBits;
        args.redBits      = getPixelFormat().redBits;
        args.greenBits    = getPixelFormat().greenBits;
        args.blueBits     = getPixelFormat().blueBits;

        scene.triangles.swap(triangles);

        if (verifyTriangleGroupRasterization(resultImage, scene, args, m_testCtx.getLog(), tcu::VERIFICATIONMODE_WEAK))
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Incorrect rendering");
    }

    return STOP;
}

void CullingTest::generateVertices(std::vector<tcu::Vec4> &outData) const
{
    de::Random rnd(543210);

    outData.resize(6);
    for (int vtxNdx = 0; vtxNdx < (int)outData.size(); ++vtxNdx)
    {
        outData[vtxNdx].x() = rnd.getFloat(-0.9f, 0.9f);
        outData[vtxNdx].y() = rnd.getFloat(-0.9f, 0.9f);
        outData[vtxNdx].z() = 0.0f;
        outData[vtxNdx].w() = 1.0f;
    }
}

void CullingTest::extractTriangles(std::vector<TriangleSceneSpec::SceneTriangle> &outTriangles,
                                   const std::vector<tcu::Vec4> &vertices) const
{
    const bool cullDirection = (m_cullMode == GL_FRONT) ^ (m_faceOrder == GL_CCW);

    // No triangles
    if (m_cullMode == GL_FRONT_AND_BACK)
        return;

    switch (m_primitive)
    {
    case GL_TRIANGLES:
    {
        for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 2; vtxNdx += 3)
        {
            const tcu::Vec4 &v0 = vertices[vtxNdx + 0];
            const tcu::Vec4 &v1 = vertices[vtxNdx + 1];
            const tcu::Vec4 &v2 = vertices[vtxNdx + 2];

            if (triangleOrder(v0, v1, v2) != cullDirection)
            {
                TriangleSceneSpec::SceneTriangle tri;
                tri.positions[0]  = v0;
                tri.sharedEdge[0] = false;
                tri.positions[1]  = v1;
                tri.sharedEdge[1] = false;
                tri.positions[2]  = v2;
                tri.sharedEdge[2] = false;

                outTriangles.push_back(tri);
            }
        }
        break;
    }

    case GL_TRIANGLE_STRIP:
    {
        for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 2; ++vtxNdx)
        {
            const tcu::Vec4 &v0 = vertices[vtxNdx + 0];
            const tcu::Vec4 &v1 = vertices[vtxNdx + 1];
            const tcu::Vec4 &v2 = vertices[vtxNdx + 2];

            if (triangleOrder(v0, v1, v2) != (cullDirection ^ (vtxNdx % 2 != 0)))
            {
                TriangleSceneSpec::SceneTriangle tri;
                tri.positions[0]  = v0;
                tri.sharedEdge[0] = false;
                tri.positions[1]  = v1;
                tri.sharedEdge[1] = false;
                tri.positions[2]  = v2;
                tri.sharedEdge[2] = false;

                outTriangles.push_back(tri);
            }
        }
        break;
    }

    case GL_TRIANGLE_FAN:
    {
        for (int vtxNdx = 1; vtxNdx < (int)vertices.size() - 1; ++vtxNdx)
        {
            const tcu::Vec4 &v0 = vertices[0];
            const tcu::Vec4 &v1 = vertices[vtxNdx + 0];
            const tcu::Vec4 &v2 = vertices[vtxNdx + 1];

            if (triangleOrder(v0, v1, v2) != cullDirection)
            {
                TriangleSceneSpec::SceneTriangle tri;
                tri.positions[0]  = v0;
                tri.sharedEdge[0] = false;
                tri.positions[1]  = v1;
                tri.sharedEdge[1] = false;
                tri.positions[2]  = v2;
                tri.sharedEdge[2] = false;

                outTriangles.push_back(tri);
            }
        }
        break;
    }

    default:
        DE_ASSERT(false);
    }
}

bool CullingTest::triangleOrder(const tcu::Vec4 &v0, const tcu::Vec4 &v1, const tcu::Vec4 &v2) const
{
    const tcu::Vec2 s0 = v0.swizzle(0, 1) / v0.w();
    const tcu::Vec2 s1 = v1.swizzle(0, 1) / v1.w();
    const tcu::Vec2 s2 = v2.swizzle(0, 1) / v2.w();

    // cross
    return ((s1.x() - s0.x()) * (s2.y() - s0.y()) - (s2.x() - s0.x()) * (s1.y() - s0.y())) < 0;
}

class TriangleInterpolationTest : public BaseRenderingCase
{
public:
    TriangleInterpolationTest(Context &ctx, const char *name, const char *desc, glw::GLenum primitive, int flags,
                              RenderTarget renderTarget = RENDERTARGET_DEFAULT, int numSamples = -1);
    ~TriangleInterpolationTest(void);
    IterateResult iterate(void);

private:
    void generateVertices(int iteration, std::vector<tcu::Vec4> &outVertices, std::vector<tcu::Vec4> &outColors) const;
    void extractTriangles(std::vector<TriangleSceneSpec::SceneTriangle> &outTriangles,
                          const std::vector<tcu::Vec4> &vertices, const std::vector<tcu::Vec4> &colors) const;

    const glw::GLenum m_primitive;
    const bool m_projective;
    const int m_iterationCount;

    int m_iteration;
    bool m_allIterationsPassed;
};

TriangleInterpolationTest::TriangleInterpolationTest(Context &ctx, const char *name, const char *desc,
                                                     glw::GLenum primitive, int flags, RenderTarget renderTarget,
                                                     int numSamples)
    : BaseRenderingCase(ctx, name, desc, renderTarget, numSamples, DEFAULT_RENDER_SIZE)
    , m_primitive(primitive)
    , m_projective((flags & INTERPOLATIONFLAGS_PROJECTED) != 0)
    , m_iterationCount(3)
    , m_iteration(0)
    , m_allIterationsPassed(true)
{
    m_flatshade = ((flags & INTERPOLATIONFLAGS_FLATSHADE) != 0);
}

TriangleInterpolationTest::~TriangleInterpolationTest(void)
{
    deinit();
}

TriangleInterpolationTest::IterateResult TriangleInterpolationTest::iterate(void)
{
    const std::string iterationDescription =
        "Test iteration " + de::toString(m_iteration + 1) + " / " + de::toString(m_iterationCount);
    const tcu::ScopedLogSection section(m_testCtx.getLog(), "Iteration" + de::toString(m_iteration + 1),
                                        iterationDescription);
    tcu::Surface resultImage(m_renderSize, m_renderSize);
    std::vector<tcu::Vec4> drawBuffer;
    std::vector<tcu::Vec4> colorBuffer;
    std::vector<TriangleSceneSpec::SceneTriangle> triangles;

    // generate scene
    generateVertices(m_iteration, drawBuffer, colorBuffer);
    extractTriangles(triangles, drawBuffer, colorBuffer);

    // log
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Generated vertices:" << tcu::TestLog::EndMessage;
        for (int vtxNdx = 0; vtxNdx < (int)drawBuffer.size(); ++vtxNdx)
            m_testCtx.getLog() << tcu::TestLog::Message << "\t" << drawBuffer[vtxNdx]
                               << ",\tcolor= " << colorBuffer[vtxNdx] << tcu::TestLog::EndMessage;
    }

    // draw image
    drawPrimitives(resultImage, drawBuffer, colorBuffer, m_primitive);

    // compare
    {
        RasterizationArguments args;
        TriangleSceneSpec scene;

        args.numSamples   = m_numSamples;
        args.subpixelBits = m_subpixelBits;
        args.redBits      = getPixelFormat().redBits;
        args.greenBits    = getPixelFormat().greenBits;
        args.blueBits     = getPixelFormat().blueBits;

        scene.triangles.swap(triangles);

        if (!verifyTriangleGroupInterpolation(resultImage, scene, args, m_testCtx.getLog()))
            m_allIterationsPassed = false;
    }

    // result
    if (++m_iteration == m_iterationCount)
    {
        if (m_allIterationsPassed)
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Found invalid pixel values");

        return STOP;
    }
    else
        return CONTINUE;
}

void TriangleInterpolationTest::generateVertices(int iteration, std::vector<tcu::Vec4> &outVertices,
                                                 std::vector<tcu::Vec4> &outColors) const
{
    // use only red, green and blue
    const tcu::Vec4 colors[] = {
        tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
    };

    de::Random rnd(123 + iteration * 1000 + (int)m_primitive);

    outVertices.resize(6);
    outColors.resize(6);

    for (int vtxNdx = 0; vtxNdx < (int)outVertices.size(); ++vtxNdx)
    {
        outVertices[vtxNdx].x() = rnd.getFloat(-0.9f, 0.9f);
        outVertices[vtxNdx].y() = rnd.getFloat(-0.9f, 0.9f);
        outVertices[vtxNdx].z() = 0.0f;

        if (!m_projective)
            outVertices[vtxNdx].w() = 1.0f;
        else
        {
            const float w = rnd.getFloat(0.2f, 4.0f);

            outVertices[vtxNdx].x() *= w;
            outVertices[vtxNdx].y() *= w;
            outVertices[vtxNdx].z() *= w;
            outVertices[vtxNdx].w() = w;
        }

        outColors[vtxNdx] = colors[vtxNdx % DE_LENGTH_OF_ARRAY(colors)];
    }
}

void TriangleInterpolationTest::extractTriangles(std::vector<TriangleSceneSpec::SceneTriangle> &outTriangles,
                                                 const std::vector<tcu::Vec4> &vertices,
                                                 const std::vector<tcu::Vec4> &colors) const
{
    switch (m_primitive)
    {
    case GL_TRIANGLES:
    {
        for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 2; vtxNdx += 3)
        {
            TriangleSceneSpec::SceneTriangle tri;
            tri.positions[0]  = vertices[vtxNdx + 0];
            tri.positions[1]  = vertices[vtxNdx + 1];
            tri.positions[2]  = vertices[vtxNdx + 2];
            tri.sharedEdge[0] = false;
            tri.sharedEdge[1] = false;
            tri.sharedEdge[2] = false;

            if (m_flatshade)
            {
                tri.colors[0] = colors[vtxNdx + 2];
                tri.colors[1] = colors[vtxNdx + 2];
                tri.colors[2] = colors[vtxNdx + 2];
            }
            else
            {
                tri.colors[0] = colors[vtxNdx + 0];
                tri.colors[1] = colors[vtxNdx + 1];
                tri.colors[2] = colors[vtxNdx + 2];
            }

            outTriangles.push_back(tri);
        }
        break;
    }

    case GL_TRIANGLE_STRIP:
    {
        for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 2; ++vtxNdx)
        {
            TriangleSceneSpec::SceneTriangle tri;
            tri.positions[0]  = vertices[vtxNdx + 0];
            tri.positions[1]  = vertices[vtxNdx + 1];
            tri.positions[2]  = vertices[vtxNdx + 2];
            tri.sharedEdge[0] = false;
            tri.sharedEdge[1] = false;
            tri.sharedEdge[2] = false;

            if (m_flatshade)
            {
                tri.colors[0] = colors[vtxNdx + 2];
                tri.colors[1] = colors[vtxNdx + 2];
                tri.colors[2] = colors[vtxNdx + 2];
            }
            else
            {
                tri.colors[0] = colors[vtxNdx + 0];
                tri.colors[1] = colors[vtxNdx + 1];
                tri.colors[2] = colors[vtxNdx + 2];
            }

            outTriangles.push_back(tri);
        }
        break;
    }

    case GL_TRIANGLE_FAN:
    {
        for (int vtxNdx = 1; vtxNdx < (int)vertices.size() - 1; ++vtxNdx)
        {
            TriangleSceneSpec::SceneTriangle tri;
            tri.positions[0]  = vertices[0];
            tri.positions[1]  = vertices[vtxNdx + 0];
            tri.positions[2]  = vertices[vtxNdx + 1];
            tri.sharedEdge[0] = false;
            tri.sharedEdge[1] = false;
            tri.sharedEdge[2] = false;

            if (m_flatshade)
            {
                tri.colors[0] = colors[vtxNdx + 1];
                tri.colors[1] = colors[vtxNdx + 1];
                tri.colors[2] = colors[vtxNdx + 1];
            }
            else
            {
                tri.colors[0] = colors[0];
                tri.colors[1] = colors[vtxNdx + 0];
                tri.colors[2] = colors[vtxNdx + 1];
            }

            outTriangles.push_back(tri);
        }
        break;
    }

    default:
        DE_ASSERT(false);
    }
}

class LineInterpolationTest : public BaseRenderingCase
{
public:
    LineInterpolationTest(Context &ctx, const char *name, const char *desc, glw::GLenum primitive, int flags,
                          PrimitiveWideness wideness, RenderTarget renderTarget = RENDERTARGET_DEFAULT,
                          int numSamples = -1);
    ~LineInterpolationTest(void);

    void init(void);
    IterateResult iterate(void);

private:
    void generateVertices(int iteration, std::vector<tcu::Vec4> &outVertices, std::vector<tcu::Vec4> &outColors) const;
    void extractLines(std::vector<LineSceneSpec::SceneLine> &outLines, const std::vector<tcu::Vec4> &vertices,
                      const std::vector<tcu::Vec4> &colors) const;
    float getLineWidth(void) const;

    const glw::GLenum m_primitive;
    const bool m_projective;
    const int m_iterationCount;
    const PrimitiveWideness m_primitiveWideness;

    int m_iteration;
    tcu::ResultCollector m_result;
    float m_maxLineWidth;
    std::vector<float> m_lineWidths;
};

LineInterpolationTest::LineInterpolationTest(Context &ctx, const char *name, const char *desc, glw::GLenum primitive,
                                             int flags, PrimitiveWideness wideness, RenderTarget renderTarget,
                                             int numSamples)
    : BaseRenderingCase(ctx, name, desc, renderTarget, numSamples, DEFAULT_RENDER_SIZE)
    , m_primitive(primitive)
    , m_projective((flags & INTERPOLATIONFLAGS_PROJECTED) != 0)
    , m_iterationCount(3)
    , m_primitiveWideness(wideness)
    , m_iteration(0)
    , m_maxLineWidth(1.0f)
{
    m_flatshade = ((flags & INTERPOLATIONFLAGS_FLATSHADE) != 0);
}

LineInterpolationTest::~LineInterpolationTest(void)
{
    deinit();
}

void LineInterpolationTest::init(void)
{
    // create line widths
    if (m_primitiveWideness == PRIMITIVEWIDENESS_NARROW)
    {
        m_lineWidths.resize(m_iterationCount, 1.0f);
    }
    else if (m_primitiveWideness == PRIMITIVEWIDENESS_WIDE)
    {
        float range[2] = {0.0f, 0.0f};
        m_context.getRenderContext().getFunctions().getFloatv(GL_ALIASED_LINE_WIDTH_RANGE, range);

        m_testCtx.getLog() << tcu::TestLog::Message << "ALIASED_LINE_WIDTH_RANGE = [" << range[0] << ", " << range[1]
                           << "]" << tcu::TestLog::EndMessage;

        // no wide line support
        if (range[1] <= 1.0f)
            throw tcu::NotSupportedError("wide line support required");

        // set hand picked sizes
        m_lineWidths.push_back(5.0f);
        m_lineWidths.push_back(10.0f);
        m_lineWidths.push_back(range[1]);
        DE_ASSERT((int)m_lineWidths.size() == m_iterationCount);

        m_maxLineWidth = range[1];
    }
    else
        DE_ASSERT(false);

    // init parent
    BaseRenderingCase::init();
}

LineInterpolationTest::IterateResult LineInterpolationTest::iterate(void)
{
    const std::string iterationDescription =
        "Test iteration " + de::toString(m_iteration + 1) + " / " + de::toString(m_iterationCount);
    const tcu::ScopedLogSection section(m_testCtx.getLog(), "Iteration" + de::toString(m_iteration + 1),
                                        iterationDescription);
    const float lineWidth = getLineWidth();
    tcu::Surface resultImage(m_renderSize, m_renderSize);
    std::vector<tcu::Vec4> drawBuffer;
    std::vector<tcu::Vec4> colorBuffer;
    std::vector<LineSceneSpec::SceneLine> lines;

    // supported?
    if (lineWidth <= m_maxLineWidth)
    {
        // generate scene
        generateVertices(m_iteration, drawBuffer, colorBuffer);
        extractLines(lines, drawBuffer, colorBuffer);

        // log
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Generated vertices:" << tcu::TestLog::EndMessage;
            for (int vtxNdx = 0; vtxNdx < (int)drawBuffer.size(); ++vtxNdx)
                m_testCtx.getLog() << tcu::TestLog::Message << "\t" << drawBuffer[vtxNdx]
                                   << ",\tcolor= " << colorBuffer[vtxNdx] << tcu::TestLog::EndMessage;
        }

        // draw image
        drawPrimitives(resultImage, drawBuffer, colorBuffer, m_primitive);

        // compare
        {
            RasterizationArguments args;
            LineSceneSpec scene;
            LineInterpolationMethod iterationResult;

            args.numSamples   = m_numSamples;
            args.subpixelBits = m_subpixelBits;
            args.redBits      = getPixelFormat().redBits;
            args.greenBits    = getPixelFormat().greenBits;
            args.blueBits     = getPixelFormat().blueBits;

            scene.lines.swap(lines);
            scene.lineWidth                      = getLineWidth();
            scene.stippleFactor                  = 1;
            scene.stipplePattern                 = 0xFFFF;
            scene.allowNonProjectedInterpolation = true;

            iterationResult = verifyLineGroupInterpolation(resultImage, scene, args, m_testCtx.getLog());
            switch (iterationResult)
            {
            case tcu::LINEINTERPOLATION_STRICTLY_CORRECT:
                // line interpolation matches the specification
                m_result.addResult(QP_TEST_RESULT_PASS, "Pass");
                break;

            case tcu::LINEINTERPOLATION_PROJECTED:
                // line interpolation weights are otherwise correct, but they are projected onto major axis
                m_testCtx.getLog() << tcu::TestLog::Message
                                   << "Interpolation was calculated using coordinates projected onto major axis. "
                                      "This method does not produce the same values as the non-projecting method "
                                      "defined in the specification."
                                   << tcu::TestLog::EndMessage;
                m_result.addResult(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Interpolation was calculated using projected coordinateds");
                break;

            case tcu::LINEINTERPOLATION_INCORRECT:
                if (scene.lineWidth != 1.0f && m_numSamples > 1)
                {
                    // multisampled wide lines might not be supported
                    m_result.addResult(QP_TEST_RESULT_COMPATIBILITY_WARNING,
                                       "Interpolation of multisampled wide lines failed");
                }
                else
                {
                    // line interpolation is incorrect
                    m_result.addResult(QP_TEST_RESULT_FAIL, "Found invalid pixel values");
                }
                break;

            default:
                DE_ASSERT(false);
                break;
            }
        }
    }
    else
        m_testCtx.getLog() << tcu::TestLog::Message << "Line width " << lineWidth
                           << " not supported, skipping iteration." << tcu::TestLog::EndMessage;

    // result
    if (++m_iteration == m_iterationCount)
    {
        m_result.setTestContextResult(m_testCtx);
        return STOP;
    }
    else
        return CONTINUE;
}

void LineInterpolationTest::generateVertices(int iteration, std::vector<tcu::Vec4> &outVertices,
                                             std::vector<tcu::Vec4> &outColors) const
{
    // use only red, green and blue
    const tcu::Vec4 colors[] = {
        tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
    };

    de::Random rnd(123 + iteration * 1000 + (int)m_primitive);

    outVertices.resize(6);
    outColors.resize(6);

    for (int vtxNdx = 0; vtxNdx < (int)outVertices.size(); ++vtxNdx)
    {
        outVertices[vtxNdx].x() = rnd.getFloat(-0.9f, 0.9f);
        outVertices[vtxNdx].y() = rnd.getFloat(-0.9f, 0.9f);
        outVertices[vtxNdx].z() = 0.0f;

        if (!m_projective)
            outVertices[vtxNdx].w() = 1.0f;
        else
        {
            const float w = rnd.getFloat(0.2f, 4.0f);

            outVertices[vtxNdx].x() *= w;
            outVertices[vtxNdx].y() *= w;
            outVertices[vtxNdx].z() *= w;
            outVertices[vtxNdx].w() = w;
        }

        outColors[vtxNdx] = colors[vtxNdx % DE_LENGTH_OF_ARRAY(colors)];
    }
}

void LineInterpolationTest::extractLines(std::vector<LineSceneSpec::SceneLine> &outLines,
                                         const std::vector<tcu::Vec4> &vertices,
                                         const std::vector<tcu::Vec4> &colors) const
{
    switch (m_primitive)
    {
    case GL_LINES:
    {
        for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 1; vtxNdx += 2)
        {
            LineSceneSpec::SceneLine line;
            line.positions[0] = vertices[vtxNdx + 0];
            line.positions[1] = vertices[vtxNdx + 1];

            if (m_flatshade)
            {
                line.colors[0] = colors[vtxNdx + 1];
                line.colors[1] = colors[vtxNdx + 1];
            }
            else
            {
                line.colors[0] = colors[vtxNdx + 0];
                line.colors[1] = colors[vtxNdx + 1];
            }

            outLines.push_back(line);
        }
        break;
    }

    case GL_LINE_STRIP:
    {
        for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 1; ++vtxNdx)
        {
            LineSceneSpec::SceneLine line;
            line.positions[0] = vertices[vtxNdx + 0];
            line.positions[1] = vertices[vtxNdx + 1];

            if (m_flatshade)
            {
                line.colors[0] = colors[vtxNdx + 1];
                line.colors[1] = colors[vtxNdx + 1];
            }
            else
            {
                line.colors[0] = colors[vtxNdx + 0];
                line.colors[1] = colors[vtxNdx + 1];
            }

            outLines.push_back(line);
        }
        break;
    }

    case GL_LINE_LOOP:
    {
        for (int vtxNdx = 0; vtxNdx < (int)vertices.size(); ++vtxNdx)
        {
            LineSceneSpec::SceneLine line;
            line.positions[0] = vertices[(vtxNdx + 0) % (int)vertices.size()];
            line.positions[1] = vertices[(vtxNdx + 1) % (int)vertices.size()];

            if (m_flatshade)
            {
                line.colors[0] = colors[(vtxNdx + 1) % (int)vertices.size()];
                line.colors[1] = colors[(vtxNdx + 1) % (int)vertices.size()];
            }
            else
            {
                line.colors[0] = colors[(vtxNdx + 0) % (int)vertices.size()];
                line.colors[1] = colors[(vtxNdx + 1) % (int)vertices.size()];
            }

            outLines.push_back(line);
        }
        break;
    }

    default:
        DE_ASSERT(false);
    }
}

float LineInterpolationTest::getLineWidth(void) const
{
    return m_lineWidths[m_iteration];
}

} // namespace

RasterizationTests::RasterizationTests(Context &context)
    : TestCaseGroup(context, "rasterization", "Rasterization Tests")
{
}

RasterizationTests::~RasterizationTests(void)
{
}

void RasterizationTests::init(void)
{
    // .primitives
    {
        tcu::TestCaseGroup *const primitives =
            new tcu::TestCaseGroup(m_testCtx, "primitives", "Primitive rasterization");

        addChild(primitives);

        primitives->addChild(new TrianglesCase(m_context, "triangles",
                                               "Render primitives as GL_TRIANGLES, verify rasterization result"));
        primitives->addChild(new TriangleStripCase(
            m_context, "triangle_strip", "Render primitives as GL_TRIANGLE_STRIP, verify rasterization result"));
        primitives->addChild(new TriangleFanCase(m_context, "triangle_fan",
                                                 "Render primitives as GL_TRIANGLE_FAN, verify rasterization result"));
        primitives->addChild(new LinesCase(m_context, "lines",
                                           "Render primitives as GL_LINES, verify rasterization result",
                                           PRIMITIVEWIDENESS_NARROW));
        primitives->addChild(new LineStripCase(m_context, "line_strip",
                                               "Render primitives as GL_LINE_STRIP, verify rasterization result",
                                               PRIMITIVEWIDENESS_NARROW));
        primitives->addChild(new LineLoopCase(m_context, "line_loop",
                                              "Render primitives as GL_LINE_LOOP, verify rasterization result",
                                              PRIMITIVEWIDENESS_NARROW));
        primitives->addChild(new LinesCase(m_context, "lines_wide",
                                           "Render primitives as GL_LINES with wide lines, verify rasterization result",
                                           PRIMITIVEWIDENESS_WIDE));
        primitives->addChild(new LineStripCase(
            m_context, "line_strip_wide",
            "Render primitives as GL_LINE_STRIP with wide lines, verify rasterization result", PRIMITIVEWIDENESS_WIDE));
        primitives->addChild(new LineLoopCase(
            m_context, "line_loop_wide",
            "Render primitives as GL_LINE_LOOP with wide lines, verify rasterization result", PRIMITIVEWIDENESS_WIDE));
        primitives->addChild(new PointCase(m_context, "points",
                                           "Render primitives as GL_POINTS, verify rasterization result",
                                           PRIMITIVEWIDENESS_WIDE));
    }

    // .fill_rules
    {
        tcu::TestCaseGroup *const fillRules = new tcu::TestCaseGroup(m_testCtx, "fill_rules", "Primitive fill rules");

        addChild(fillRules);

        fillRules->addChild(
            new FillRuleCase(m_context, "basic_quad", "Verify fill rules", FillRuleCase::FILLRULECASE_BASIC));
        fillRules->addChild(new FillRuleCase(m_context, "basic_quad_reverse", "Verify fill rules",
                                             FillRuleCase::FILLRULECASE_REVERSED));
        fillRules->addChild(
            new FillRuleCase(m_context, "clipped_full", "Verify fill rules", FillRuleCase::FILLRULECASE_CLIPPED_FULL));
        fillRules->addChild(new FillRuleCase(m_context, "clipped_partly", "Verify fill rules",
                                             FillRuleCase::FILLRULECASE_CLIPPED_PARTIAL));
        fillRules->addChild(
            new FillRuleCase(m_context, "projected", "Verify fill rules", FillRuleCase::FILLRULECASE_PROJECTED));
    }

    // .culling
    {
        static const struct CullMode
        {
            glw::GLenum mode;
            const char *prefix;
        } cullModes[] = {
            {GL_FRONT, "front_"},
            {GL_BACK, "back_"},
            {GL_FRONT_AND_BACK, "both_"},
        };
        static const struct PrimitiveType
        {
            glw::GLenum type;
            const char *name;
        } primitiveTypes[] = {
            {GL_TRIANGLES, "triangles"},
            {GL_TRIANGLE_STRIP, "triangle_strip"},
            {GL_TRIANGLE_FAN, "triangle_fan"},
        };
        static const struct FrontFaceOrder
        {
            glw::GLenum mode;
            const char *postfix;
        } frontOrders[] = {
            {GL_CCW, ""},
            {GL_CW, "_reverse"},
        };

        tcu::TestCaseGroup *const culling = new tcu::TestCaseGroup(m_testCtx, "culling", "Culling");

        addChild(culling);

        for (int cullModeNdx = 0; cullModeNdx < DE_LENGTH_OF_ARRAY(cullModes); ++cullModeNdx)
            for (int primitiveNdx = 0; primitiveNdx < DE_LENGTH_OF_ARRAY(primitiveTypes); ++primitiveNdx)
                for (int frontOrderNdx = 0; frontOrderNdx < DE_LENGTH_OF_ARRAY(frontOrders); ++frontOrderNdx)
                {
                    const std::string name = std::string(cullModes[cullModeNdx].prefix) +
                                             primitiveTypes[primitiveNdx].name + frontOrders[frontOrderNdx].postfix;

                    culling->addChild(new CullingTest(m_context, name.c_str(), "Test primitive culling.",
                                                      cullModes[cullModeNdx].mode, primitiveTypes[primitiveNdx].type,
                                                      frontOrders[frontOrderNdx].mode));
                }
    }

    // .interpolation
    {
        tcu::TestCaseGroup *const interpolation =
            new tcu::TestCaseGroup(m_testCtx, "interpolation", "Test interpolation");

        addChild(interpolation);

        // .basic
        {
            tcu::TestCaseGroup *const basic =
                new tcu::TestCaseGroup(m_testCtx, "basic", "Non-projective interpolation");

            interpolation->addChild(basic);

            basic->addChild(new TriangleInterpolationTest(m_context, "triangles", "Verify triangle interpolation",
                                                          GL_TRIANGLES, INTERPOLATIONFLAGS_NONE));
            basic->addChild(new TriangleInterpolationTest(m_context, "triangle_strip",
                                                          "Verify triangle strip interpolation", GL_TRIANGLE_STRIP,
                                                          INTERPOLATIONFLAGS_NONE));
            basic->addChild(new TriangleInterpolationTest(m_context, "triangle_fan",
                                                          "Verify triangle fan interpolation", GL_TRIANGLE_FAN,
                                                          INTERPOLATIONFLAGS_NONE));
            basic->addChild(new LineInterpolationTest(m_context, "lines", "Verify line interpolation", GL_LINES,
                                                      INTERPOLATIONFLAGS_NONE, PRIMITIVEWIDENESS_NARROW));
            basic->addChild(new LineInterpolationTest(m_context, "line_strip", "Verify line strip interpolation",
                                                      GL_LINE_STRIP, INTERPOLATIONFLAGS_NONE,
                                                      PRIMITIVEWIDENESS_NARROW));
            basic->addChild(new LineInterpolationTest(m_context, "line_loop", "Verify line loop interpolation",
                                                      GL_LINE_LOOP, INTERPOLATIONFLAGS_NONE, PRIMITIVEWIDENESS_NARROW));
            basic->addChild(new LineInterpolationTest(m_context, "lines_wide", "Verify wide line interpolation",
                                                      GL_LINES, INTERPOLATIONFLAGS_NONE, PRIMITIVEWIDENESS_WIDE));
            basic->addChild(new LineInterpolationTest(m_context, "line_strip_wide",
                                                      "Verify wide line strip interpolation", GL_LINE_STRIP,
                                                      INTERPOLATIONFLAGS_NONE, PRIMITIVEWIDENESS_WIDE));
            basic->addChild(new LineInterpolationTest(m_context, "line_loop_wide",
                                                      "Verify wide line loop interpolation", GL_LINE_LOOP,
                                                      INTERPOLATIONFLAGS_NONE, PRIMITIVEWIDENESS_WIDE));
        }

        // .projected
        {
            tcu::TestCaseGroup *const projected =
                new tcu::TestCaseGroup(m_testCtx, "projected", "Projective interpolation");

            interpolation->addChild(projected);

            projected->addChild(new TriangleInterpolationTest(m_context, "triangles", "Verify triangle interpolation",
                                                              GL_TRIANGLES, INTERPOLATIONFLAGS_PROJECTED));
            projected->addChild(new TriangleInterpolationTest(m_context, "triangle_strip",
                                                              "Verify triangle strip interpolation", GL_TRIANGLE_STRIP,
                                                              INTERPOLATIONFLAGS_PROJECTED));
            projected->addChild(new TriangleInterpolationTest(m_context, "triangle_fan",
                                                              "Verify triangle fan interpolation", GL_TRIANGLE_FAN,
                                                              INTERPOLATIONFLAGS_PROJECTED));
            projected->addChild(new LineInterpolationTest(m_context, "lines", "Verify line interpolation", GL_LINES,
                                                          INTERPOLATIONFLAGS_PROJECTED, PRIMITIVEWIDENESS_NARROW));
            projected->addChild(new LineInterpolationTest(m_context, "line_strip", "Verify line strip interpolation",
                                                          GL_LINE_STRIP, INTERPOLATIONFLAGS_PROJECTED,
                                                          PRIMITIVEWIDENESS_NARROW));
            projected->addChild(new LineInterpolationTest(m_context, "line_loop", "Verify line loop interpolation",
                                                          GL_LINE_LOOP, INTERPOLATIONFLAGS_PROJECTED,
                                                          PRIMITIVEWIDENESS_NARROW));
            projected->addChild(new LineInterpolationTest(m_context, "lines_wide", "Verify wide line interpolation",
                                                          GL_LINES, INTERPOLATIONFLAGS_PROJECTED,
                                                          PRIMITIVEWIDENESS_WIDE));
            projected->addChild(new LineInterpolationTest(m_context, "line_strip_wide",
                                                          "Verify wide line strip interpolation", GL_LINE_STRIP,
                                                          INTERPOLATIONFLAGS_PROJECTED, PRIMITIVEWIDENESS_WIDE));
            projected->addChild(new LineInterpolationTest(m_context, "line_loop_wide",
                                                          "Verify wide line loop interpolation", GL_LINE_LOOP,
                                                          INTERPOLATIONFLAGS_PROJECTED, PRIMITIVEWIDENESS_WIDE));
        }
    }

    // .flatshading
    {
        tcu::TestCaseGroup *const flatshading = new tcu::TestCaseGroup(m_testCtx, "flatshading", "Test flatshading");

        addChild(flatshading);

        flatshading->addChild(new TriangleInterpolationTest(m_context, "triangles", "Verify triangle flatshading",
                                                            GL_TRIANGLES, INTERPOLATIONFLAGS_FLATSHADE));
        flatshading->addChild(new TriangleInterpolationTest(m_context, "triangle_strip",
                                                            "Verify triangle strip flatshading", GL_TRIANGLE_STRIP,
                                                            INTERPOLATIONFLAGS_FLATSHADE));
        flatshading->addChild(new TriangleInterpolationTest(m_context, "triangle_fan",
                                                            "Verify triangle fan flatshading", GL_TRIANGLE_FAN,
                                                            INTERPOLATIONFLAGS_FLATSHADE));
        flatshading->addChild(new LineInterpolationTest(m_context, "lines", "Verify line flatshading", GL_LINES,
                                                        INTERPOLATIONFLAGS_FLATSHADE, PRIMITIVEWIDENESS_NARROW));
        flatshading->addChild(new LineInterpolationTest(m_context, "line_strip", "Verify line strip flatshading",
                                                        GL_LINE_STRIP, INTERPOLATIONFLAGS_FLATSHADE,
                                                        PRIMITIVEWIDENESS_NARROW));
        flatshading->addChild(new LineInterpolationTest(m_context, "line_loop", "Verify line loop flatshading",
                                                        GL_LINE_LOOP, INTERPOLATIONFLAGS_FLATSHADE,
                                                        PRIMITIVEWIDENESS_NARROW));
        flatshading->addChild(new LineInterpolationTest(m_context, "lines_wide", "Verify wide line flatshading",
                                                        GL_LINES, INTERPOLATIONFLAGS_FLATSHADE,
                                                        PRIMITIVEWIDENESS_WIDE));
        flatshading->addChild(new LineInterpolationTest(m_context, "line_strip_wide",
                                                        "Verify wide line strip flatshading", GL_LINE_STRIP,
                                                        INTERPOLATIONFLAGS_FLATSHADE, PRIMITIVEWIDENESS_WIDE));
        flatshading->addChild(new LineInterpolationTest(m_context, "line_loop_wide",
                                                        "Verify wide line loop flatshading", GL_LINE_LOOP,
                                                        INTERPOLATIONFLAGS_FLATSHADE, PRIMITIVEWIDENESS_WIDE));
    }

    // .fbo
    {
        static const struct
        {
            const char *name;
            BaseRenderingCase::RenderTarget target;
            int numSamples;
        } renderTargets[] = {
            {"texture_2d", BaseRenderingCase::RENDERTARGET_TEXTURE_2D, -1},
            {"rbo_singlesample", BaseRenderingCase::RENDERTARGET_RBO_SINGLESAMPLE, -1},
            {"rbo_multisample_4", BaseRenderingCase::RENDERTARGET_RBO_MULTISAMPLE, 4},
            {"rbo_multisample_max", BaseRenderingCase::RENDERTARGET_RBO_MULTISAMPLE,
             BaseRenderingCase::SAMPLE_COUNT_MAX},
        };

        tcu::TestCaseGroup *const fboGroup = new tcu::TestCaseGroup(m_testCtx, "fbo", "Test using framebuffer objects");
        addChild(fboGroup);

        // .texture_2d
        // .rbo_singlesample
        // .rbo_multisample_4
        // .rbo_multisample_max
        for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(renderTargets); ++targetNdx)
        {
            tcu::TestCaseGroup *const colorAttachmentGroup = new tcu::TestCaseGroup(
                m_testCtx, renderTargets[targetNdx].name,
                ("Test using " + std::string(renderTargets[targetNdx].name) + " color attachment").c_str());
            fboGroup->addChild(colorAttachmentGroup);

            // .primitives
            {
                tcu::TestCaseGroup *const primitiveGroup =
                    new tcu::TestCaseGroup(m_testCtx, "primitives", "Primitive rasterization");
                colorAttachmentGroup->addChild(primitiveGroup);

                primitiveGroup->addChild(new TrianglesCase(
                    m_context, "triangles", "Render primitives as GL_TRIANGLES, verify rasterization result",
                    renderTargets[targetNdx].target, renderTargets[targetNdx].numSamples));
                primitiveGroup->addChild(new LinesCase(
                    m_context, "lines", "Render primitives as GL_LINES, verify rasterization result",
                    PRIMITIVEWIDENESS_NARROW, renderTargets[targetNdx].target, renderTargets[targetNdx].numSamples));
                primitiveGroup->addChild(new LinesCase(
                    m_context, "lines_wide",
                    "Render primitives as GL_LINES with wide lines, verify rasterization result",
                    PRIMITIVEWIDENESS_WIDE, renderTargets[targetNdx].target, renderTargets[targetNdx].numSamples));
                primitiveGroup->addChild(new PointCase(
                    m_context, "points", "Render primitives as GL_POINTS, verify rasterization result",
                    PRIMITIVEWIDENESS_WIDE, renderTargets[targetNdx].target, renderTargets[targetNdx].numSamples));
            }

            // .fill_rules
            {
                tcu::TestCaseGroup *const fillRules =
                    new tcu::TestCaseGroup(m_testCtx, "fill_rules", "Primitive fill rules");

                colorAttachmentGroup->addChild(fillRules);

                fillRules->addChild(new FillRuleCase(m_context, "basic_quad", "Verify fill rules",
                                                     FillRuleCase::FILLRULECASE_BASIC, renderTargets[targetNdx].target,
                                                     renderTargets[targetNdx].numSamples));
                fillRules->addChild(new FillRuleCase(
                    m_context, "basic_quad_reverse", "Verify fill rules", FillRuleCase::FILLRULECASE_REVERSED,
                    renderTargets[targetNdx].target, renderTargets[targetNdx].numSamples));
                fillRules->addChild(new FillRuleCase(
                    m_context, "clipped_full", "Verify fill rules", FillRuleCase::FILLRULECASE_CLIPPED_FULL,
                    renderTargets[targetNdx].target, renderTargets[targetNdx].numSamples));
                fillRules->addChild(new FillRuleCase(
                    m_context, "clipped_partly", "Verify fill rules", FillRuleCase::FILLRULECASE_CLIPPED_PARTIAL,
                    renderTargets[targetNdx].target, renderTargets[targetNdx].numSamples));
                fillRules->addChild(
                    new FillRuleCase(m_context, "projected", "Verify fill rules", FillRuleCase::FILLRULECASE_PROJECTED,
                                     renderTargets[targetNdx].target, renderTargets[targetNdx].numSamples));
            }

            // .interpolation
            {
                tcu::TestCaseGroup *const interpolation =
                    new tcu::TestCaseGroup(m_testCtx, "interpolation", "Non-projective interpolation");

                colorAttachmentGroup->addChild(interpolation);

                interpolation->addChild(new TriangleInterpolationTest(
                    m_context, "triangles", "Verify triangle interpolation", GL_TRIANGLES, INTERPOLATIONFLAGS_NONE,
                    renderTargets[targetNdx].target, renderTargets[targetNdx].numSamples));
                interpolation->addChild(new LineInterpolationTest(
                    m_context, "lines", "Verify line interpolation", GL_LINES, INTERPOLATIONFLAGS_NONE,
                    PRIMITIVEWIDENESS_NARROW, renderTargets[targetNdx].target, renderTargets[targetNdx].numSamples));
                interpolation->addChild(new LineInterpolationTest(
                    m_context, "lines_wide", "Verify wide line interpolation", GL_LINES, INTERPOLATIONFLAGS_NONE,
                    PRIMITIVEWIDENESS_WIDE, renderTargets[targetNdx].target, renderTargets[targetNdx].numSamples));
            }
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
