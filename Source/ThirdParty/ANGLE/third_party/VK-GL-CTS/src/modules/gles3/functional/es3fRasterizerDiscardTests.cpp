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
 * \brief Rasterizer discard tests.
 *//*--------------------------------------------------------------------*/

#include "es3fRasterizerDiscardTests.hpp"

#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuSurface.hpp"
#include "tcuRenderTarget.hpp"
#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deString.h"

#include "glw.h"

using tcu::TestLog;
using tcu::Vec4;

namespace deqp
{
namespace gles3
{
namespace Functional
{

static const int NUM_CASE_ITERATIONS = 1;
static const Vec4 FAIL_COLOR_RED     = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
static const Vec4 PASS_COLOR_BLUE    = Vec4(0.0f, 0.0f, 0.5f, 1.0f);
static const Vec4 BLACK_COLOR        = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
static const float FAIL_DEPTH        = 0.0f;
static const int FAIL_STENCIL        = 1;
static const float UNIT_SQUARE[16]   = {1.0f,  1.0f, 0.05f, 1.0f, 1.0f,  -1.0f, 0.05f, 1.0f,
                                        -1.0f, 1.0f, 0.05f, 1.0f, -1.0f, -1.0f, 0.05f, 1.0f};

enum CaseType
{
    CASE_WRITE_DEPTH,
    CASE_WRITE_STENCIL,
    CASE_CLEAR_COLOR,
    CASE_CLEAR_DEPTH,
    CASE_CLEAR_STENCIL
};

enum CaseOptions
{
    CASEOPTION_FBO     = (1 << 0),
    CASEOPTION_SCISSOR = (1 << 1)
};

class RasterizerDiscardCase : public TestCase
{
public:
    RasterizerDiscardCase(Context &context, const char *name, const char *description, int numPrimitives,
                          CaseType caseType, uint32_t caseOptions, uint32_t drawMode = GL_TRIANGLES);
    ~RasterizerDiscardCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    RasterizerDiscardCase(const RasterizerDiscardCase &other);
    RasterizerDiscardCase &operator=(const RasterizerDiscardCase &other);

    void setupFramebufferObject(void);
    void deleteFramebufferObject(void);

    int m_numPrimitives;
    CaseType m_caseType;
    uint32_t m_caseOptions;
    uint32_t m_drawMode;

    glu::ShaderProgram *m_program;
    uint32_t m_fbo;
    uint32_t m_colorBuf;
    uint32_t m_depthStencilBuf;
    int m_iterNdx;
    de::Random m_rnd;
};

RasterizerDiscardCase::RasterizerDiscardCase(Context &context, const char *name, const char *description,
                                             int numPrimitives, CaseType caseType, uint32_t caseOptions,
                                             uint32_t drawMode)
    : TestCase(context, name, description)
    , m_numPrimitives(numPrimitives)
    , m_caseType(caseType)
    , m_caseOptions(caseOptions)
    , m_drawMode(drawMode)
    , m_program(nullptr)
    , m_fbo(0)
    , m_colorBuf(0)
    , m_depthStencilBuf(0)
    , m_iterNdx(0)
    , m_rnd(deStringHash(name))
{
}

RasterizerDiscardCase::~RasterizerDiscardCase(void)
{
    RasterizerDiscardCase::deinit();
}

static void generateVertices(std::vector<float> &dst, int numPrimitives, de::Random &rnd, uint32_t drawMode)
{
    int numVertices;

    switch (drawMode)
    {
    case GL_POINTS:
        numVertices = numPrimitives;
        break;
    case GL_LINES:
        numVertices = 2 * numPrimitives;
        break;
    case GL_LINE_STRIP:
        numVertices = numPrimitives + 1;
        break;
    case GL_LINE_LOOP:
        numVertices = numPrimitives + 2;
        break;
    case GL_TRIANGLES:
        numVertices = 3 * numPrimitives;
        break;
    case GL_TRIANGLE_STRIP:
        numVertices = numPrimitives + 2;
        break;
    case GL_TRIANGLE_FAN:
        numVertices = numPrimitives + 2;
        break;
    default:
        DE_ASSERT(false);
        numVertices = 0;
    }

    dst.resize(numVertices * 4);

    for (int i = 0; i < numVertices; i++)
    {
        dst[i * 4]     = rnd.getFloat(-1.0f, 1.0f); // x
        dst[i * 4 + 1] = rnd.getFloat(-1.0f, 1.0f); // y
        dst[i * 4 + 2] = rnd.getFloat(0.1f, 0.9f);  // z
        dst[i * 4 + 3] = 1.0f;                      // w
    }
}

void RasterizerDiscardCase::setupFramebufferObject(void)
{
    int width  = m_context.getRenderTarget().getWidth();
    int height = m_context.getRenderTarget().getHeight();

    // Create framebuffer object

    glGenFramebuffers(1, &m_fbo);              // FBO
    glGenTextures(1, &m_colorBuf);             // Color attachment
    glGenRenderbuffers(1, &m_depthStencilBuf); // Depth and stencil attachments

    // Create color texture

    glBindTexture(GL_TEXTURE_2D, m_colorBuf);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Create depth and stencil buffers

    glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuf);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

    // Attach texture and buffers to FBO

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorBuf, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuf);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuf);

    uint32_t fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (fboStatus == GL_FRAMEBUFFER_UNSUPPORTED)
        throw tcu::NotSupportedError("Framebuffer unsupported", "", __FILE__, __LINE__);
    else if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
        throw tcu::TestError("Failed to create framebuffer object", "", __FILE__, __LINE__);
}

void RasterizerDiscardCase::deleteFramebufferObject(void)
{
    glDeleteTextures(1, &m_colorBuf);             // Color attachment
    glDeleteRenderbuffers(1, &m_depthStencilBuf); // Depth and stencil attachments
    glDeleteFramebuffers(1, &m_fbo);              // FBO
}

void RasterizerDiscardCase::init(void)
{
    const char *vertShaderSource = "#version 300 es\n"
                                   "layout(location = 0) in mediump vec4 a_position;\n"
                                   "\n"
                                   "void main (void)\n"
                                   "{\n"
                                   "    gl_Position = a_position;\n"
                                   "}\n";

    const char *fragShaderSource = "#version 300 es\n"
                                   "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                   "uniform mediump vec4 u_color;\n"
                                   "\n"
                                   "void main (void)\n"
                                   "{\n"
                                   "    mediump float depth_gradient = gl_FragCoord.z;\n"
                                   "    mediump float bias = 0.1;\n"
                                   "    dEQP_FragColor = vec4(u_color.xyz * (depth_gradient + bias), 1.0);\n"
                                   "}\n";

    DE_ASSERT(!m_program);
    m_program = new glu::ShaderProgram(m_context.getRenderContext(),
                                       glu::makeVtxFragSources(vertShaderSource, fragShaderSource));

    if (!m_program->isOk())
    {
        m_testCtx.getLog() << *m_program;
        TCU_FAIL("Failed to compile shader program");
    }

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass"); // Initialize test result to pass.
    GLU_CHECK_MSG("Case initialization finished");
}

void RasterizerDiscardCase::deinit(void)
{
    deleteFramebufferObject();
    delete m_program;
    m_program = nullptr;
}

RasterizerDiscardCase::IterateResult RasterizerDiscardCase::iterate(void)
{
    TestLog &log                          = m_testCtx.getLog();
    const tcu::RenderTarget &renderTarget = m_context.getRenderTarget();
    uint32_t colorUnif                    = glGetUniformLocation(m_program->getProgram(), "u_color");
    bool failColorFound                   = false;
    bool passColorFound                   = false;
    std::vector<float> vertices;

    std::string header = "Case iteration " + de::toString(m_iterNdx + 1) + " / " + de::toString(NUM_CASE_ITERATIONS);
    log << TestLog::Section(header, header);

    DE_ASSERT(m_program);

    // Create and bind FBO if needed

    if (m_caseOptions & CASEOPTION_FBO)
    {
        try
        {
            setupFramebufferObject();
        }
        catch (tcu::NotSupportedError &e)
        {
            log << TestLog::Message << "ERROR: " << e.what() << "." << TestLog::EndMessage << TestLog::EndSection;
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
            return STOP;
        }
        catch (tcu::InternalError &e)
        {
            log << TestLog::Message << "ERROR: " << e.what() << "." << TestLog::EndMessage << TestLog::EndSection;
            m_testCtx.setTestResult(QP_TEST_RESULT_INTERNAL_ERROR, "Error");
            return STOP;
        }
    }

    if (m_caseOptions & CASEOPTION_SCISSOR)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, renderTarget.getWidth(), renderTarget.getHeight());
        log << TestLog::Message << "Scissor test enabled: glScissor(0, 0, " << renderTarget.getWidth() << ", "
            << renderTarget.getHeight() << ")" << TestLog::EndMessage;
    }

    glUseProgram(m_program->getProgram());

    glEnable(GL_DEPTH_TEST);
    glDepthRangef(0.0f, 1.0f);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    glStencilOp(GL_REPLACE, GL_KEEP, GL_KEEP);

    glClearColor(PASS_COLOR_BLUE.x(), PASS_COLOR_BLUE.y(), PASS_COLOR_BLUE.z(), PASS_COLOR_BLUE.w());
    glClearDepthf(1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Generate vertices

    glEnableVertexAttribArray(0);
    generateVertices(vertices, m_numPrimitives, m_rnd, m_drawMode);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, &vertices[0]);

    // Clear color to black for depth and stencil clear cases

    if (m_caseType == CASE_CLEAR_DEPTH || m_caseType == CASE_CLEAR_STENCIL)
    {
        glClearColor(BLACK_COLOR.x(), BLACK_COLOR.y(), BLACK_COLOR.z(), BLACK_COLOR.w());
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Set fail values for color, depth and stencil

    glUniform4f(colorUnif, FAIL_COLOR_RED.x(), FAIL_COLOR_RED.y(), FAIL_COLOR_RED.z(), FAIL_COLOR_RED.w());
    glClearColor(FAIL_COLOR_RED.x(), FAIL_COLOR_RED.y(), FAIL_COLOR_RED.z(), FAIL_COLOR_RED.w());
    glClearDepthf(FAIL_DEPTH);
    glClearStencil(FAIL_STENCIL);

    // Enable rasterizer discard

    glEnable(GL_RASTERIZER_DISCARD);
    GLU_CHECK_MSG("Rasterizer discard enabled");

    // Do to-be-discarded primitive draws and buffer clears

    switch (m_caseType)
    {
    case CASE_WRITE_DEPTH:
        glDrawArrays(m_drawMode, 0, (int)vertices.size() / 4);
        break;
    case CASE_WRITE_STENCIL:
        glDrawArrays(m_drawMode, 0, (int)vertices.size() / 4);
        break;
    case CASE_CLEAR_COLOR:
        (m_caseOptions & CASEOPTION_FBO) ? glClearBufferfv(GL_COLOR, 0, &FAIL_COLOR_RED[0]) :
                                           glClear(GL_COLOR_BUFFER_BIT);
        break;
    case CASE_CLEAR_DEPTH:
        (m_caseOptions & CASEOPTION_FBO) ? glClearBufferfv(GL_DEPTH, 0, &FAIL_DEPTH) : glClear(GL_DEPTH_BUFFER_BIT);
        break;
    case CASE_CLEAR_STENCIL:
        (m_caseOptions & CASEOPTION_FBO) ? glClearBufferiv(GL_STENCIL, 0, &FAIL_STENCIL) :
                                           glClear(GL_STENCIL_BUFFER_BIT);
        break;
    default:
        DE_ASSERT(false);
    }

    // Disable rasterizer discard

    glDisable(GL_RASTERIZER_DISCARD);
    GLU_CHECK_MSG("Rasterizer discard disabled");

    if (m_caseType == CASE_WRITE_STENCIL)
    {
        if ((m_caseOptions & CASEOPTION_FBO) || m_context.getRenderTarget().getStencilBits() > 0)
        {
            // Draw a full-screen square that colors all pixels red if they have stencil value 1.

            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, &UNIT_SQUARE[0]);
            glStencilFunc(GL_EQUAL, 1, 0xFF);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        // \note If no stencil buffers are present and test is rendering to default framebuffer, test will always pass.
    }
    else if (m_caseType == CASE_CLEAR_DEPTH || m_caseType == CASE_CLEAR_STENCIL)
    {
        // Draw pass-indicating primitives for depth and stencil clear cases

        glUniform4f(colorUnif, PASS_COLOR_BLUE.x(), PASS_COLOR_BLUE.y(), PASS_COLOR_BLUE.z(), PASS_COLOR_BLUE.w());
        glDrawArrays(m_drawMode, 0, (int)vertices.size() / 4);
    }

    glFinish();
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

    // Read and check pixel data

    tcu::Surface pixels(renderTarget.getWidth(), renderTarget.getHeight());
    glu::readPixels(m_context.getRenderContext(), 0, 0, pixels.getAccess());

    {
        int width  = pixels.getWidth();
        int height = pixels.getHeight();

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                if (pixels.getPixel(x, y).getBlue() != 0)
                    passColorFound = true;

                if (pixels.getPixel(x, y).getRed() != 0)
                {
                    failColorFound = true;
                    break;
                }
            }
            if (failColorFound)
                break;
        }
    }

    // Delete FBO if created

    if (m_caseOptions & CASEOPTION_FBO)
        deleteFramebufferObject();

    // Evaluate test result

    bool testOk = passColorFound && !failColorFound;

    if (!testOk)
        log << TestLog::Image("Result image", "Result image", pixels);
    log << TestLog::Message << "Test result: " << (testOk ? "Passed!" : "Failed!") << TestLog::EndMessage;

    if (!testOk)
    {
        log << TestLog::Message << "Primitive or buffer clear was not discarded." << TestLog::EndMessage
            << TestLog::EndSection;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
    }

    log << TestLog::Message << "Primitive or buffer clear was discarded correctly." << TestLog::EndMessage
        << TestLog::EndSection;

    return (++m_iterNdx < NUM_CASE_ITERATIONS) ? CONTINUE : STOP;
}

RasterizerDiscardTests::RasterizerDiscardTests(Context &context)
    : TestCaseGroup(context, "rasterizer_discard", "Rasterizer Discard Tests")
{
}

RasterizerDiscardTests::~RasterizerDiscardTests(void)
{
}

void RasterizerDiscardTests::init(void)
{
    tcu::TestCaseGroup *basic =
        new tcu::TestCaseGroup(m_testCtx, "basic", "Rasterizer discard test for default framebuffer");
    tcu::TestCaseGroup *scissor = new tcu::TestCaseGroup(
        m_testCtx, "scissor", "Rasterizer discard test for default framebuffer with scissor test enabled");
    tcu::TestCaseGroup *fbo =
        new tcu::TestCaseGroup(m_testCtx, "fbo", "Rasterizer discard test for framebuffer object");

    addChild(basic);
    addChild(scissor);
    addChild(fbo);

    // Default framebuffer cases

    basic->addChild(
        new RasterizerDiscardCase(m_context, "write_depth_points", "points", 4, CASE_WRITE_DEPTH, 0, GL_POINTS));
    basic->addChild(
        new RasterizerDiscardCase(m_context, "write_depth_lines", "lines", 4, CASE_WRITE_DEPTH, 0, GL_LINES));
    basic->addChild(new RasterizerDiscardCase(m_context, "write_depth_line_strip", "line_strip", 4, CASE_WRITE_DEPTH, 0,
                                              GL_LINE_STRIP));
    basic->addChild(new RasterizerDiscardCase(m_context, "write_depth_line_loop", "line_loop", 4, CASE_WRITE_DEPTH, 0,
                                              GL_LINE_LOOP));
    basic->addChild(new RasterizerDiscardCase(m_context, "write_depth_triangles", "triangles", 4, CASE_WRITE_DEPTH, 0,
                                              GL_TRIANGLES));
    basic->addChild(new RasterizerDiscardCase(m_context, "write_depth_triangle_strip", "triangle_strip", 4,
                                              CASE_WRITE_DEPTH, 0, GL_TRIANGLE_STRIP));
    basic->addChild(new RasterizerDiscardCase(m_context, "write_depth_triangle_fan", "triangle_fan", 4,
                                              CASE_WRITE_DEPTH, 0, GL_TRIANGLE_FAN));

    basic->addChild(
        new RasterizerDiscardCase(m_context, "write_stencil_points", "points", 4, CASE_WRITE_STENCIL, 0, GL_POINTS));
    basic->addChild(
        new RasterizerDiscardCase(m_context, "write_stencil_lines", "lines", 4, CASE_WRITE_STENCIL, 0, GL_LINES));
    basic->addChild(new RasterizerDiscardCase(m_context, "write_stencil_line_strip", "line_strip", 4,
                                              CASE_WRITE_STENCIL, 0, GL_LINE_STRIP));
    basic->addChild(new RasterizerDiscardCase(m_context, "write_stencil_line_loop", "line_loop", 4, CASE_WRITE_STENCIL,
                                              0, GL_LINE_LOOP));
    basic->addChild(new RasterizerDiscardCase(m_context, "write_stencil_triangles", "triangles", 4, CASE_WRITE_STENCIL,
                                              0, GL_TRIANGLES));
    basic->addChild(new RasterizerDiscardCase(m_context, "write_stencil_triangle_strip", "triangle_strip", 4,
                                              CASE_WRITE_STENCIL, 0, GL_TRIANGLE_STRIP));
    basic->addChild(new RasterizerDiscardCase(m_context, "write_stencil_triangle_fan", "triangle_fan", 4,
                                              CASE_WRITE_STENCIL, 0, GL_TRIANGLE_FAN));

    basic->addChild(new RasterizerDiscardCase(m_context, "clear_color", "clear_color", 4, CASE_CLEAR_COLOR, 0));
    basic->addChild(new RasterizerDiscardCase(m_context, "clear_depth", "clear_depth", 4, CASE_CLEAR_DEPTH, 0));
    basic->addChild(new RasterizerDiscardCase(m_context, "clear_stencil", "clear_stencil", 4, CASE_CLEAR_STENCIL, 0));

    // Default framebuffer cases with scissor test enabled

    scissor->addChild(new RasterizerDiscardCase(m_context, "write_depth_points", "points", 4, CASE_WRITE_DEPTH,
                                                CASEOPTION_SCISSOR, GL_POINTS));
    scissor->addChild(new RasterizerDiscardCase(m_context, "write_depth_lines", "lines", 4, CASE_WRITE_DEPTH,
                                                CASEOPTION_SCISSOR, GL_LINES));
    scissor->addChild(new RasterizerDiscardCase(m_context, "write_depth_line_strip", "line_strip", 4, CASE_WRITE_DEPTH,
                                                CASEOPTION_SCISSOR, GL_LINE_STRIP));
    scissor->addChild(new RasterizerDiscardCase(m_context, "write_depth_line_loop", "line_loop", 4, CASE_WRITE_DEPTH,
                                                CASEOPTION_SCISSOR, GL_LINE_LOOP));
    scissor->addChild(new RasterizerDiscardCase(m_context, "write_depth_triangles", "triangles", 4, CASE_WRITE_DEPTH,
                                                CASEOPTION_SCISSOR, GL_TRIANGLES));
    scissor->addChild(new RasterizerDiscardCase(m_context, "write_depth_triangle_strip", "triangle_strip", 4,
                                                CASE_WRITE_DEPTH, CASEOPTION_SCISSOR, GL_TRIANGLE_STRIP));
    scissor->addChild(new RasterizerDiscardCase(m_context, "write_depth_triangle_fan", "triangle_fan", 4,
                                                CASE_WRITE_DEPTH, CASEOPTION_SCISSOR, GL_TRIANGLE_FAN));

    scissor->addChild(new RasterizerDiscardCase(m_context, "write_stencil_points", "points", 4, CASE_WRITE_STENCIL,
                                                CASEOPTION_SCISSOR, GL_POINTS));
    scissor->addChild(new RasterizerDiscardCase(m_context, "write_stencil_lines", "lines", 4, CASE_WRITE_STENCIL,
                                                CASEOPTION_SCISSOR, GL_LINES));
    scissor->addChild(new RasterizerDiscardCase(m_context, "write_stencil_line_strip", "line_strip", 4,
                                                CASE_WRITE_STENCIL, CASEOPTION_SCISSOR, GL_LINE_STRIP));
    scissor->addChild(new RasterizerDiscardCase(m_context, "write_stencil_line_loop", "line_loop", 4,
                                                CASE_WRITE_STENCIL, CASEOPTION_SCISSOR, GL_LINE_LOOP));
    scissor->addChild(new RasterizerDiscardCase(m_context, "write_stencil_triangles", "triangles", 4,
                                                CASE_WRITE_STENCIL, CASEOPTION_SCISSOR, GL_TRIANGLES));
    scissor->addChild(new RasterizerDiscardCase(m_context, "write_stencil_triangle_strip", "triangle_strip", 4,
                                                CASE_WRITE_STENCIL, CASEOPTION_SCISSOR, GL_TRIANGLE_STRIP));
    scissor->addChild(new RasterizerDiscardCase(m_context, "write_stencil_triangle_fan", "triangle_fan", 4,
                                                CASE_WRITE_STENCIL, CASEOPTION_SCISSOR, GL_TRIANGLE_FAN));

    scissor->addChild(
        new RasterizerDiscardCase(m_context, "clear_color", "clear_color", 4, CASE_CLEAR_COLOR, CASEOPTION_SCISSOR));
    scissor->addChild(
        new RasterizerDiscardCase(m_context, "clear_depth", "clear_depth", 4, CASE_CLEAR_DEPTH, CASEOPTION_SCISSOR));
    scissor->addChild(new RasterizerDiscardCase(m_context, "clear_stencil", "clear_stencil", 4, CASE_CLEAR_STENCIL,
                                                CASEOPTION_SCISSOR));

    // FBO cases

    fbo->addChild(new RasterizerDiscardCase(m_context, "write_depth_points", "points", 4, CASE_WRITE_DEPTH,
                                            CASEOPTION_FBO, GL_POINTS));
    fbo->addChild(new RasterizerDiscardCase(m_context, "write_depth_lines", "lines", 4, CASE_WRITE_DEPTH,
                                            CASEOPTION_FBO, GL_LINES));
    fbo->addChild(new RasterizerDiscardCase(m_context, "write_depth_line_strip", "line_strip", 4, CASE_WRITE_DEPTH,
                                            CASEOPTION_FBO, GL_LINE_STRIP));
    fbo->addChild(new RasterizerDiscardCase(m_context, "write_depth_line_loop", "line_loop", 4, CASE_WRITE_DEPTH,
                                            CASEOPTION_FBO, GL_LINE_LOOP));
    fbo->addChild(new RasterizerDiscardCase(m_context, "write_depth_triangles", "triangles", 4, CASE_WRITE_DEPTH,
                                            CASEOPTION_FBO, GL_TRIANGLES));
    fbo->addChild(new RasterizerDiscardCase(m_context, "write_depth_triangle_strip", "triangle_strip", 4,
                                            CASE_WRITE_DEPTH, CASEOPTION_FBO, GL_TRIANGLE_STRIP));
    fbo->addChild(new RasterizerDiscardCase(m_context, "write_depth_triangle_fan", "triangle_fan", 4, CASE_WRITE_DEPTH,
                                            CASEOPTION_FBO, GL_TRIANGLE_FAN));

    fbo->addChild(new RasterizerDiscardCase(m_context, "write_stencil_points", "points", 4, CASE_WRITE_STENCIL,
                                            CASEOPTION_FBO, GL_POINTS));
    fbo->addChild(new RasterizerDiscardCase(m_context, "write_stencil_lines", "lines", 4, CASE_WRITE_STENCIL,
                                            CASEOPTION_FBO, GL_LINES));
    fbo->addChild(new RasterizerDiscardCase(m_context, "write_stencil_line_strip", "line_strip", 4, CASE_WRITE_STENCIL,
                                            CASEOPTION_FBO, GL_LINE_STRIP));
    fbo->addChild(new RasterizerDiscardCase(m_context, "write_stencil_line_loop", "line_loop", 4, CASE_WRITE_STENCIL,
                                            CASEOPTION_FBO, GL_LINE_LOOP));
    fbo->addChild(new RasterizerDiscardCase(m_context, "write_stencil_triangles", "triangles", 4, CASE_WRITE_STENCIL,
                                            CASEOPTION_FBO, GL_TRIANGLES));
    fbo->addChild(new RasterizerDiscardCase(m_context, "write_stencil_triangle_strip", "triangle_strip", 4,
                                            CASE_WRITE_STENCIL, CASEOPTION_FBO, GL_TRIANGLE_STRIP));
    fbo->addChild(new RasterizerDiscardCase(m_context, "write_stencil_triangle_fan", "triangle_fan", 4,
                                            CASE_WRITE_STENCIL, CASEOPTION_FBO, GL_TRIANGLE_FAN));

    fbo->addChild(
        new RasterizerDiscardCase(m_context, "clear_color", "clear_color", 4, CASE_CLEAR_COLOR, CASEOPTION_FBO));
    fbo->addChild(
        new RasterizerDiscardCase(m_context, "clear_depth", "clear_depth", 4, CASE_CLEAR_DEPTH, CASEOPTION_FBO));
    fbo->addChild(
        new RasterizerDiscardCase(m_context, "clear_stencil", "clear_stencil", 4, CASE_CLEAR_STENCIL, CASEOPTION_FBO));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
