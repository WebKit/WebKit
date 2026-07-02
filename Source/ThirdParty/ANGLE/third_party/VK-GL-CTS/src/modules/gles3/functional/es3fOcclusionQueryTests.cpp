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
 * \brief Occlusion query tests.
 *//*--------------------------------------------------------------------*/

#include "es3fOcclusionQueryTests.hpp"

#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuSurface.hpp"
#include "tcuRenderTarget.hpp"
#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "deRandom.hpp"
#include "deString.h"

#include "glw.h"

namespace deqp
{
namespace gles3
{
namespace Functional
{

static const tcu::Vec4 DEPTH_WRITE_COLOR   = tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
static const tcu::Vec4 DEPTH_CLEAR_COLOR   = tcu::Vec4(0.0f, 0.5f, 0.8f, 1.0f);
static const tcu::Vec4 STENCIL_WRITE_COLOR = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
static const tcu::Vec4 STENCIL_CLEAR_COLOR = tcu::Vec4(0.0f, 0.8f, 0.5f, 1.0f);
static const tcu::Vec4 TARGET_COLOR        = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
static const int ELEMENTS_PER_VERTEX       = 4;
static const int NUM_CASE_ITERATIONS       = 10;

// Constants to tweak visible/invisible case probability balance.

static const int DEPTH_CLEAR_OFFSET   = 100;
static const int STENCIL_CLEAR_OFFSET = 100;
static const int SCISSOR_OFFSET       = 100;
static const int SCISSOR_MINSIZE      = 250;

enum OccluderType
{
    OCCLUDER_SCISSOR       = (1 << 0),
    OCCLUDER_DEPTH_WRITE   = (1 << 1),
    OCCLUDER_DEPTH_CLEAR   = (1 << 2),
    OCCLUDER_STENCIL_WRITE = (1 << 3),
    OCCLUDER_STENCIL_CLEAR = (1 << 4)
};

class OcclusionQueryCase : public TestCase
{
public:
    OcclusionQueryCase(Context &context, const char *name, const char *description, int numOccluderDraws,
                       int numOccludersPerDraw, float occluderSize, int numTargetDraws, int numTargetsPerDraw,
                       float targetSize, uint32_t queryMode, uint32_t occluderTypes);
    ~OcclusionQueryCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    OcclusionQueryCase(const OcclusionQueryCase &other);
    OcclusionQueryCase &operator=(const OcclusionQueryCase &other);

    int m_numOccluderDraws;
    int m_numOccludersPerDraw;
    float m_occluderSize;
    int m_numTargetDraws;
    int m_numTargetsPerDraw;
    float m_targetSize;
    uint32_t m_queryMode;
    uint32_t m_occluderTypes;

    glu::RenderContext &m_renderCtx;
    glu::ShaderProgram *m_program;
    int m_iterNdx;
    de::Random m_rnd;
};

OcclusionQueryCase::OcclusionQueryCase(Context &context, const char *name, const char *description,
                                       int numOccluderDraws, int numOccludersPerDraw, float occluderSize,
                                       int numTargetDraws, int numTargetsPerDraw, float targetSize, uint32_t queryMode,
                                       uint32_t occluderTypes)
    : TestCase(context, name, description)
    , m_numOccluderDraws(numOccluderDraws)
    , m_numOccludersPerDraw(numOccludersPerDraw)
    , m_occluderSize(occluderSize)
    , m_numTargetDraws(numTargetDraws)
    , m_numTargetsPerDraw(numTargetsPerDraw)
    , m_targetSize(targetSize)
    , m_queryMode(queryMode)
    , m_occluderTypes(occluderTypes)
    , m_renderCtx(context.getRenderContext())
    , m_program(nullptr)
    , m_iterNdx(0)
    , m_rnd(deStringHash(name))
{
}

OcclusionQueryCase::~OcclusionQueryCase(void)
{
    OcclusionQueryCase::deinit();
}

static void generateVertices(std::vector<float> &dst, float width, float height, int primitiveCount,
                             int verticesPerPrimitive, de::Random rnd, float primitiveSize, float minZ, float maxZ)
{
    float w = width / 2.0f;
    float h = height / 2.0f;
    float s = primitiveSize / 2.0f;

    int vertexCount = verticesPerPrimitive * primitiveCount;
    dst.resize(vertexCount * ELEMENTS_PER_VERTEX);

    for (int i = 0; i < vertexCount; i += 3) // First loop gets a random point inside unit square
    {
        float rndX = rnd.getFloat(-w, w);
        float rndY = rnd.getFloat(-h, h);

        for (int j = 0; j < verticesPerPrimitive;
             j++) // Second loop gets 3 random points within given distance s from (rndX, rndY)
        {
            dst[(i + j) * ELEMENTS_PER_VERTEX]     = rndX + rnd.getFloat(-s, s); // x
            dst[(i + j) * ELEMENTS_PER_VERTEX + 1] = rndY + rnd.getFloat(-s, s); // y
            dst[(i + j) * ELEMENTS_PER_VERTEX + 2] = rnd.getFloat(minZ, maxZ);   // z
            dst[(i + j) * ELEMENTS_PER_VERTEX + 3] = 1.0f;                       // w
        }
    }
}

void OcclusionQueryCase::init(void)
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
                                   "    mediump float depth_gradient = max(gl_FragCoord.z, 0.0);\n"
                                   "    mediump float bias = 0.1;\n"
                                   "    dEQP_FragColor = vec4(u_color.xyz * (depth_gradient + bias), 1.0);\n"
                                   "}\n";

    DE_ASSERT(!m_program);
    m_program = new glu::ShaderProgram(m_context.getRenderContext(),
                                       glu::makeVtxFragSources(vertShaderSource, fragShaderSource));

    if (!m_program->isOk())
    {
        m_testCtx.getLog() << *m_program;
        delete m_program;
        m_program = nullptr;
        TCU_FAIL("Failed to compile shader program");
    }

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass"); // Initialize test result to pass.
    GLU_CHECK_MSG("Case initialization finished");
}

void OcclusionQueryCase::deinit(void)
{
    delete m_program;
    m_program = nullptr;
}

OcclusionQueryCase::IterateResult OcclusionQueryCase::iterate(void)
{
    tcu::TestLog &log                     = m_testCtx.getLog();
    const tcu::RenderTarget &renderTarget = m_context.getRenderTarget();
    uint32_t colorUnif                    = glGetUniformLocation(m_program->getProgram(), "u_color");

    std::vector<float> occluderVertices;
    std::vector<float> targetVertices;
    std::vector<uint32_t> queryIds(1, 0);
    bool queryResult     = false;
    bool colorReadResult = false;
    int targetW          = renderTarget.getWidth();
    int targetH          = renderTarget.getHeight();

    log << tcu::TestLog::Message << "Case iteration " << m_iterNdx + 1 << " / " << NUM_CASE_ITERATIONS
        << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "Parameters:\n"
        << "- " << m_numOccluderDraws << " occluder draws, " << m_numOccludersPerDraw << " primitive writes per draw,\n"
        << "- " << m_numTargetDraws << " target draws, " << m_numTargetsPerDraw << " targets per draw\n"
        << tcu::TestLog::EndMessage;

    DE_ASSERT(m_program);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glUseProgram(m_program->getProgram());
    glEnableVertexAttribArray(0);

    // Draw occluders

    std::vector<OccluderType> occOptions(0);
    if (m_occluderTypes & OCCLUDER_DEPTH_WRITE)
        occOptions.push_back(OCCLUDER_DEPTH_WRITE);
    if (m_occluderTypes & OCCLUDER_DEPTH_CLEAR)
        occOptions.push_back(OCCLUDER_DEPTH_CLEAR);
    if (m_occluderTypes & OCCLUDER_STENCIL_WRITE)
        occOptions.push_back(OCCLUDER_STENCIL_WRITE);
    if (m_occluderTypes & OCCLUDER_STENCIL_CLEAR)
        occOptions.push_back(OCCLUDER_STENCIL_CLEAR);

    for (int i = 0; i < m_numOccluderDraws; i++)
    {
        if (occOptions.empty())
            break;

        OccluderType type = occOptions[m_rnd.getInt(
            0, (int)occOptions.size() - 1)]; // Choosing a random occluder type from available options

        switch (type)
        {
        case OCCLUDER_DEPTH_WRITE:
            log << tcu::TestLog::Message << "Occluder draw " << i + 1 << " / " << m_numOccluderDraws << " : "
                << "Depth write" << tcu::TestLog::EndMessage;

            generateVertices(occluderVertices, 2.0f, 2.0f, m_numOccludersPerDraw, 3, m_rnd, m_occluderSize, 0.0f,
                             0.6f); // Generate vertices for occluding primitives

            DE_ASSERT(!occluderVertices.empty());

            glEnable(GL_DEPTH_TEST);
            glUniform4f(colorUnif, DEPTH_WRITE_COLOR.x(), DEPTH_WRITE_COLOR.y(), DEPTH_WRITE_COLOR.z(),
                        DEPTH_WRITE_COLOR.w());
            glVertexAttribPointer(0, ELEMENTS_PER_VERTEX, GL_FLOAT, GL_FALSE, 0, &occluderVertices[0]);
            glDrawArrays(GL_TRIANGLES, 0, 3 * m_numOccludersPerDraw);
            glDisable(GL_DEPTH_TEST);

            break;

        case OCCLUDER_DEPTH_CLEAR:
        {
            int scissorBoxX = m_rnd.getInt(-DEPTH_CLEAR_OFFSET, targetW);
            int scissorBoxY = m_rnd.getInt(-DEPTH_CLEAR_OFFSET, targetH);
            int scissorBoxW = m_rnd.getInt(DEPTH_CLEAR_OFFSET, targetW + DEPTH_CLEAR_OFFSET);
            int scissorBoxH = m_rnd.getInt(DEPTH_CLEAR_OFFSET, targetH + DEPTH_CLEAR_OFFSET);

            log << tcu::TestLog::Message << "Occluder draw " << i + 1 << " / " << m_numOccluderDraws << " : "
                << "Depth clear" << tcu::TestLog::EndMessage;
            log << tcu::TestLog::Message << "Depth-clearing box drawn at "
                << "(" << scissorBoxX << ", " << scissorBoxY << ")"
                << ", width = " << scissorBoxW << ", height = " << scissorBoxH << "." << tcu::TestLog::EndMessage;

            glEnable(GL_SCISSOR_TEST);
            glScissor(scissorBoxX, scissorBoxY, scissorBoxW, scissorBoxH);
            glClearDepthf(0.0f);
            glClearColor(DEPTH_CLEAR_COLOR.x(), DEPTH_CLEAR_COLOR.y(), DEPTH_CLEAR_COLOR.z(), DEPTH_CLEAR_COLOR.w());
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);

            break;
        }

        case OCCLUDER_STENCIL_WRITE:
            log << tcu::TestLog::Message << "Occluder draw " << i + 1 << " / " << m_numOccluderDraws << " : "
                << "Stencil write" << tcu::TestLog::EndMessage;

            generateVertices(occluderVertices, 2.0f, 2.0f, m_numOccludersPerDraw, 3, m_rnd, m_occluderSize, 0.0f, 0.6f);

            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

            DE_ASSERT(!occluderVertices.empty());

            glEnable(GL_STENCIL_TEST);
            glUniform4f(colorUnif, STENCIL_WRITE_COLOR.x(), STENCIL_WRITE_COLOR.y(), STENCIL_WRITE_COLOR.z(),
                        STENCIL_WRITE_COLOR.w());
            glVertexAttribPointer(0, ELEMENTS_PER_VERTEX, GL_FLOAT, GL_FALSE, 0, &occluderVertices[0]);
            glDrawArrays(GL_TRIANGLES, 0, 3 * m_numOccludersPerDraw);
            glDisable(GL_STENCIL_TEST);

            break;

        case OCCLUDER_STENCIL_CLEAR:
        {
            int scissorBoxX = m_rnd.getInt(-STENCIL_CLEAR_OFFSET, targetW);
            int scissorBoxY = m_rnd.getInt(-STENCIL_CLEAR_OFFSET, targetH);
            int scissorBoxW = m_rnd.getInt(STENCIL_CLEAR_OFFSET, targetW + STENCIL_CLEAR_OFFSET);
            int scissorBoxH = m_rnd.getInt(STENCIL_CLEAR_OFFSET, targetH + STENCIL_CLEAR_OFFSET);

            log << tcu::TestLog::Message << "Occluder draw " << i + 1 << " / " << m_numOccluderDraws << " : "
                << "Stencil clear" << tcu::TestLog::EndMessage;
            log << tcu::TestLog::Message << "Stencil-clearing box drawn at "
                << "(" << scissorBoxX << ", " << scissorBoxY << ")"
                << ", width = " << scissorBoxW << ", height = " << scissorBoxH << "." << tcu::TestLog::EndMessage;

            glEnable(GL_SCISSOR_TEST);
            glScissor(scissorBoxX, scissorBoxY, scissorBoxW, scissorBoxH);
            glClearStencil(1);
            glClearColor(STENCIL_CLEAR_COLOR.x(), STENCIL_CLEAR_COLOR.y(), STENCIL_CLEAR_COLOR.z(),
                         STENCIL_CLEAR_COLOR.w());
            glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);

            break;
        }

        default:
            DE_ASSERT(false);
            break;
        }
    }

    if (m_occluderTypes & OCCLUDER_SCISSOR)
    {
        int scissorBoxX = m_rnd.getInt(-SCISSOR_OFFSET, targetW - SCISSOR_OFFSET);
        int scissorBoxY = m_rnd.getInt(-SCISSOR_OFFSET, targetH - SCISSOR_OFFSET);
        int scissorBoxW = m_rnd.getInt(SCISSOR_MINSIZE, targetW + SCISSOR_OFFSET);
        int scissorBoxH = m_rnd.getInt(SCISSOR_MINSIZE, targetH + SCISSOR_OFFSET);

        log << tcu::TestLog::Message << "Scissor box drawn at "
            << "(" << scissorBoxX << ", " << scissorBoxY << ")"
            << ", width = " << scissorBoxW << ", height = " << scissorBoxH << "." << tcu::TestLog::EndMessage;

        glEnable(GL_SCISSOR_TEST);
        glScissor(scissorBoxX, scissorBoxY, scissorBoxW, scissorBoxH);
    }

    glGenQueries(1, &queryIds[0]);
    glBeginQuery(m_queryMode, queryIds[0]);
    GLU_CHECK_MSG("Occlusion query started");

    // Draw target primitives

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 0, 0xFF);

    for (int i = 0; i < m_numTargetDraws; i++)
    {
        generateVertices(targetVertices, 2.0f, 2.0f, m_numTargetsPerDraw, 3, m_rnd, m_targetSize, 0.4f,
                         1.0f); // Generate vertices for target primitives

        if (!targetVertices.empty())
        {
            glUniform4f(colorUnif, TARGET_COLOR.x(), TARGET_COLOR.y(), TARGET_COLOR.z(), TARGET_COLOR.w());
            glVertexAttribPointer(0, ELEMENTS_PER_VERTEX, GL_FLOAT, GL_FALSE, 0, &targetVertices[0]);
            glDrawArrays(GL_TRIANGLES, 0, 3 * m_numTargetsPerDraw);
        }
    }

    glEndQuery(m_queryMode);
    glFinish();
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);

    // Check that query result is available.
    {
        uint32_t resultAvailable = GL_FALSE;
        glGetQueryObjectuiv(queryIds[0], GL_QUERY_RESULT_AVAILABLE, &resultAvailable);

        if (resultAvailable == GL_FALSE)
            TCU_FAIL("Occlusion query failed to return a result after glFinish()");
    }

    // Read query result.
    {
        uint32_t result = 0;
        glGetQueryObjectuiv(queryIds[0], GL_QUERY_RESULT, &result);
        queryResult = (result != GL_FALSE);
    }

    glDeleteQueries(1, &queryIds[0]);
    GLU_CHECK_MSG("Occlusion query finished");

    // Read pixel data

    tcu::Surface pixels(renderTarget.getWidth(), renderTarget.getHeight());
    glu::readPixels(m_context.getRenderContext(), 0, 0, pixels.getAccess());

    {
        int width  = pixels.getWidth();
        int height = pixels.getHeight();

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                if (pixels.getPixel(x, y).getRed() != 0)
                {
                    colorReadResult = true;
                    break;
                }
            }
            if (colorReadResult)
                break;
        }
    }

    log << tcu::TestLog::Message << "Occlusion query result:  Target " << (queryResult ? "visible" : "invisible")
        << "\n"
        << "Framebuffer read result: Target " << (colorReadResult ? "visible" : "invisible")
        << tcu::TestLog::EndMessage;

    bool testOk = false;
    if (m_queryMode == GL_ANY_SAMPLES_PASSED_CONSERVATIVE)
    {
        if (queryResult || colorReadResult)
            testOk = queryResult; // Allow conservative occlusion query to return false positives.
        else
            testOk = queryResult == colorReadResult;
    }
    else
        testOk = (queryResult == colorReadResult);

    if (!testOk)
    {
        log << tcu::TestLog::Image("Result image", "Result image", pixels);
        log << tcu::TestLog::Message << "Case FAILED!" << tcu::TestLog::EndMessage;

        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
    }

    log << tcu::TestLog::Message << "Case passed!" << tcu::TestLog::EndMessage;

    return (++m_iterNdx < NUM_CASE_ITERATIONS) ? CONTINUE : STOP;
}

OcclusionQueryTests::OcclusionQueryTests(Context &context)
    : TestCaseGroup(context, "occlusion_query", "Occlusion Query Tests")
{
}

OcclusionQueryTests::~OcclusionQueryTests(void)
{
}

void OcclusionQueryTests::init(void)
{
    // Strict occlusion query cases

    addChild(new OcclusionQueryCase(m_context, "scissor", "scissor", 1, 10, 1.6f, 1, 1, 0.3f, GL_ANY_SAMPLES_PASSED,
                                    OCCLUDER_SCISSOR));
    addChild(new OcclusionQueryCase(m_context, "depth_write", "depth_write", 8, 10, 1.6f, 1, 7, 0.3f,
                                    GL_ANY_SAMPLES_PASSED, OCCLUDER_DEPTH_WRITE));
    addChild(new OcclusionQueryCase(m_context, "depth_clear", "depth_clear", 5, 10, 1.6f, 1, 5, 0.2f,
                                    GL_ANY_SAMPLES_PASSED, OCCLUDER_DEPTH_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "stencil_write", "stencil_write", 8, 10, 2.0f, 1, 5, 0.4f,
                                    GL_ANY_SAMPLES_PASSED, OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(m_context, "stencil_clear", "stencil_clear", 5, 10, 2.0f, 1, 3, 0.3f,
                                    GL_ANY_SAMPLES_PASSED, OCCLUDER_STENCIL_CLEAR));

    addChild(new OcclusionQueryCase(m_context, "scissor_depth_write", "scissor_depth_write", 5, 10, 1.6f, 2, 5, 0.3f,
                                    GL_ANY_SAMPLES_PASSED, OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE));
    addChild(new OcclusionQueryCase(m_context, "scissor_depth_clear", "scissor_depth_clear", 7, 10, 1.6f, 2, 5, 1.0f,
                                    GL_ANY_SAMPLES_PASSED, OCCLUDER_SCISSOR | OCCLUDER_DEPTH_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "scissor_stencil_write", "scissor_stencil_write", 4, 10, 1.6f, 2, 5,
                                    0.3f, GL_ANY_SAMPLES_PASSED, OCCLUDER_SCISSOR | OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(m_context, "scissor_stencil_clear", "scissor_stencil_clear", 4, 10, 1.6f, 2, 5,
                                    1.0f, GL_ANY_SAMPLES_PASSED, OCCLUDER_SCISSOR | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "depth_write_depth_clear", "depth_write_depth_clear", 7, 10, 1.6f, 1, 5,
                                    0.2f, GL_ANY_SAMPLES_PASSED, OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "depth_write_stencil_write", "depth_write_stencil_write", 8, 10, 1.6f, 1,
                                    5, 0.3f, GL_ANY_SAMPLES_PASSED, OCCLUDER_DEPTH_WRITE | OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(m_context, "depth_write_stencil_clear", "depth_write_stencil_clear", 8, 10, 1.6f, 1,
                                    5, 0.3f, GL_ANY_SAMPLES_PASSED, OCCLUDER_DEPTH_WRITE | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "depth_clear_stencil_write", "depth_clear_stencil_write", 8, 10, 1.6f, 1,
                                    5, 0.3f, GL_ANY_SAMPLES_PASSED, OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(m_context, "depth_clear_stencil_clear", "depth_clear_stencil_clear", 12, 10, 1.6f,
                                    1, 5, 0.2f, GL_ANY_SAMPLES_PASSED, OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "stencil_write_stencil_clear", "stencil_write_stencil_clear", 5, 10,
                                    2.0f, 1, 5, 0.4f, GL_ANY_SAMPLES_PASSED,
                                    OCCLUDER_STENCIL_WRITE | OCCLUDER_STENCIL_CLEAR));

    addChild(new OcclusionQueryCase(m_context, "scissor_depth_write_depth_clear", "scissor_depth_write_depth_clear", 5,
                                    10, 1.6f, 2, 5, 0.4f, GL_ANY_SAMPLES_PASSED,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "scissor_depth_write_stencil_write", "scissor_depth_write_stencil_write",
                                    4, 10, 1.6f, 2, 5, 0.4f, GL_ANY_SAMPLES_PASSED,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE | OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(m_context, "scissor_depth_write_stencil_clear", "scissor_depth_write_stencil_clear",
                                    6, 10, 1.6f, 2, 5, 0.4f, GL_ANY_SAMPLES_PASSED,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "scissor_depth_clear_stencil_write", "scissor_depth_clear_stencil_write",
                                    4, 10, 1.6f, 2, 5, 0.4f, GL_ANY_SAMPLES_PASSED,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(m_context, "scissor_depth_clear_stencil_clear", "scissor_depth_clear_stencil_clear",
                                    5, 10, 1.6f, 2, 5, 0.4f, GL_ANY_SAMPLES_PASSED,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(
        m_context, "scissor_stencil_write_stencil_clear", "scissor_stencil_write_stencil_clear", 4, 10, 1.6f, 2, 5,
        0.4f, GL_ANY_SAMPLES_PASSED, OCCLUDER_SCISSOR | OCCLUDER_STENCIL_WRITE | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(
        m_context, "depth_write_depth_clear_stencil_write", "depth_write_depth_clear_stencil_write", 7, 10, 1.6f, 2, 5,
        0.4f, GL_ANY_SAMPLES_PASSED, OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(
        m_context, "depth_write_depth_clear_stencil_clear", "depth_write_depth_clear_stencil_clear", 7, 10, 1.6f, 2, 5,
        0.4f, GL_ANY_SAMPLES_PASSED, OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(
        m_context, "depth_write_stencil_write_stencil_clear", "depth_write_stencil_write_stencil_clear", 7, 10, 1.6f, 2,
        5, 0.4f, GL_ANY_SAMPLES_PASSED, OCCLUDER_DEPTH_WRITE | OCCLUDER_STENCIL_WRITE | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(
        m_context, "depth_clear_stencil_write_stencil_clear", "depth_clear_stencil_write_stencil_clear", 7, 10, 1.6f, 2,
        5, 0.4f, GL_ANY_SAMPLES_PASSED, OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_WRITE | OCCLUDER_STENCIL_CLEAR));

    addChild(new OcclusionQueryCase(
        m_context, "scissor_depth_write_depth_clear_stencil_write", "scissor_depth_write_depth_clear_stencil_write", 4,
        10, 1.6f, 2, 5, 0.4f, GL_ANY_SAMPLES_PASSED,
        OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(
        m_context, "scissor_depth_write_depth_clear_stencil_clear", "scissor_depth_write_depth_clear_stencil_clear", 4,
        10, 1.6f, 2, 5, 0.4f, GL_ANY_SAMPLES_PASSED,
        OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(
        m_context, "scissor_depth_write_stencil_write_stencil_clear", "scissor_depth_write_stencil_write_stencil_clear",
        5, 10, 1.6f, 2, 5, 0.4f, GL_ANY_SAMPLES_PASSED,
        OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE | OCCLUDER_STENCIL_WRITE | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(
        m_context, "scissor_depth_clear_stencil_write_stencil_clear", "scissor_depth_clear_stencil_write_stencil_clear",
        4, 10, 1.6f, 2, 5, 0.4f, GL_ANY_SAMPLES_PASSED,
        OCCLUDER_SCISSOR | OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_WRITE | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(
        m_context, "depth_write_depth_clear_stencil_write_stencil_clear",
        "depth_write_depth_clear_stencil_write_stencil_clear", 7, 10, 1.6f, 2, 5, 0.4f, GL_ANY_SAMPLES_PASSED,
        OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_WRITE | OCCLUDER_STENCIL_CLEAR));

    addChild(new OcclusionQueryCase(m_context, "all_occluders", "all_occluders", 7, 10, 1.6f, 3, 5, 0.6f,
                                    GL_ANY_SAMPLES_PASSED,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR |
                                        OCCLUDER_STENCIL_WRITE | OCCLUDER_STENCIL_CLEAR));

    // Conservative occlusion query cases

    addChild(new OcclusionQueryCase(m_context, "conservative_scissor", "conservative_scissor", 1, 10, 1.6f, 1, 1, 0.3f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE, OCCLUDER_SCISSOR));
    addChild(new OcclusionQueryCase(m_context, "conservative_depth_write", "conservative_depth_write", 8, 10, 1.6f, 1,
                                    7, 0.3f, GL_ANY_SAMPLES_PASSED_CONSERVATIVE, OCCLUDER_DEPTH_WRITE));
    addChild(new OcclusionQueryCase(m_context, "conservative_depth_clear", "conservative_depth_clear", 5, 10, 1.6f, 1,
                                    5, 0.2f, GL_ANY_SAMPLES_PASSED_CONSERVATIVE, OCCLUDER_DEPTH_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "conservative_stencil_write", "conservative_stencil_write", 8, 10, 2.0f,
                                    1, 5, 0.4f, GL_ANY_SAMPLES_PASSED_CONSERVATIVE, OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(m_context, "conservative_stencil_clear", "conservative_stencil_clear", 5, 10, 2.0f,
                                    1, 3, 0.3f, GL_ANY_SAMPLES_PASSED_CONSERVATIVE, OCCLUDER_STENCIL_CLEAR));

    addChild(new OcclusionQueryCase(m_context, "conservative_scissor_depth_write", "conservative_scissor_depth_write",
                                    5, 10, 1.6f, 2, 5, 0.3f, GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE));
    addChild(new OcclusionQueryCase(m_context, "conservative_scissor_depth_clear", "conservative_scissor_depth_clear",
                                    7, 10, 1.6f, 2, 5, 1.0f, GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "conservative_scissor_stencil_write",
                                    "conservative_scissor_stencil_write", 4, 10, 1.6f, 2, 5, 0.3f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE, OCCLUDER_SCISSOR | OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(m_context, "conservative_scissor_stencil_clear",
                                    "conservative_scissor_stencil_clear", 4, 10, 1.6f, 2, 5, 1.0f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE, OCCLUDER_SCISSOR | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "conservative_depth_write_depth_clear",
                                    "conservative_depth_write_depth_clear", 7, 10, 1.6f, 1, 5, 0.2f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE, OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "conservative_depth_write_stencil_write",
                                    "conservative_depth_write_stencil_write", 8, 10, 1.6f, 1, 5, 0.3f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE, OCCLUDER_DEPTH_WRITE | OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(m_context, "conservative_depth_write_stencil_clear",
                                    "conservative_depth_write_stencil_clear", 8, 10, 1.6f, 1, 5, 0.3f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE, OCCLUDER_DEPTH_WRITE | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "conservative_depth_clear_stencil_write",
                                    "conservative_depth_clear_stencil_write", 8, 10, 1.6f, 1, 5, 0.3f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE, OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(m_context, "conservative_depth_clear_stencil_clear",
                                    "conservative_depth_clear_stencil_clear", 12, 10, 1.6f, 1, 5, 0.2f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE, OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(
        m_context, "conservative_stencil_write_stencil_clear", "conservative_stencil_write_stencil_clear", 5, 10, 2.0f,
        1, 5, 0.4f, GL_ANY_SAMPLES_PASSED_CONSERVATIVE, OCCLUDER_STENCIL_WRITE | OCCLUDER_STENCIL_CLEAR));

    addChild(new OcclusionQueryCase(m_context, "conservative_scissor_depth_write_depth_clear",
                                    "conservative_scissor_depth_write_depth_clear", 5, 10, 1.6f, 2, 5, 0.4f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "conservative_scissor_depth_write_stencil_write",
                                    "conservative_scissor_depth_write_stencil_write", 4, 10, 1.6f, 2, 5, 0.4f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE | OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(m_context, "conservative_scissor_depth_write_stencil_clear",
                                    "conservative_scissor_depth_write_stencil_clear", 6, 10, 1.6f, 2, 5, 0.4f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "conservative_scissor_depth_clear_stencil_write",
                                    "conservative_scissor_depth_clear_stencil_write", 4, 10, 1.6f, 2, 5, 0.4f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(m_context, "conservative_scissor_depth_clear_stencil_clear",
                                    "conservative_scissor_depth_clear_stencil_clear", 5, 10, 1.6f, 2, 5, 0.4f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "conservative_scissor_stencil_write_stencil_clear",
                                    "conservative_scissor_stencil_write_stencil_clear", 4, 10, 1.6f, 2, 5, 0.4f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_SCISSOR | OCCLUDER_STENCIL_WRITE | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "conservative_depth_write_depth_clear_stencil_write",
                                    "conservative_depth_write_depth_clear_stencil_write", 7, 10, 1.6f, 2, 5, 0.4f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(m_context, "conservative_depth_write_depth_clear_stencil_clear",
                                    "conservative_depth_write_depth_clear_stencil_clear", 7, 10, 1.6f, 2, 5, 0.4f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "conservative_depth_write_stencil_write_stencil_clear",
                                    "conservative_depth_write_stencil_write_stencil_clear", 7, 10, 1.6f, 2, 5, 0.4f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_DEPTH_WRITE | OCCLUDER_STENCIL_WRITE | OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "conservative_depth_clear_stencil_write_stencil_clear",
                                    "conservative_depth_clear_stencil_write_stencil_clear", 7, 10, 1.6f, 2, 5, 0.4f,
                                    GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_WRITE | OCCLUDER_STENCIL_CLEAR));

    addChild(new OcclusionQueryCase(m_context, "conservative_scissor_depth_write_depth_clear_stencil_write",
                                    "conservative_scissor_depth_write_depth_clear_stencil_write", 4, 10, 1.6f, 2, 5,
                                    0.4f, GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR |
                                        OCCLUDER_STENCIL_WRITE));
    addChild(new OcclusionQueryCase(m_context, "conservative_scissor_depth_write_depth_clear_stencil_clear",
                                    "conservative_scissor_depth_write_depth_clear_stencil_clear", 4, 10, 1.6f, 2, 5,
                                    0.4f, GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR |
                                        OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "conservative_scissor_depth_write_stencil_write_stencil_clear",
                                    "conservative_scissor_depth_write_stencil_write_stencil_clear", 5, 10, 1.6f, 2, 5,
                                    0.4f, GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE | OCCLUDER_STENCIL_WRITE |
                                        OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "conservative_scissor_depth_clear_stencil_write_stencil_clear",
                                    "conservative_scissor_depth_clear_stencil_write_stencil_clear", 4, 10, 1.6f, 2, 5,
                                    0.4f, GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_WRITE |
                                        OCCLUDER_STENCIL_CLEAR));
    addChild(new OcclusionQueryCase(m_context, "conservative_depth_write_depth_clear_stencil_write_stencil_clear",
                                    "conservative_depth_write_depth_clear_stencil_write_stencil_clear", 7, 10, 1.6f, 2,
                                    5, 0.4f, GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR | OCCLUDER_STENCIL_WRITE |
                                        OCCLUDER_STENCIL_CLEAR));

    addChild(new OcclusionQueryCase(m_context, "conservative_all_occluders", "conservative_all_occluders", 7, 10, 1.6f,
                                    3, 5, 0.6f, GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    OCCLUDER_SCISSOR | OCCLUDER_DEPTH_WRITE | OCCLUDER_DEPTH_CLEAR |
                                        OCCLUDER_STENCIL_WRITE | OCCLUDER_STENCIL_CLEAR));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
