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
 * \brief Occlusion query stress tests
 *//*--------------------------------------------------------------------*/

#include "es3sOcclusionQueryTests.hpp"

#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deString.h"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuSurface.hpp"
#include "gluShaderProgram.hpp"
#include "deClock.h"

#include "glw.h"

#include <vector>

using std::vector;
using tcu::TestLog;

namespace deqp
{
namespace gles3
{
namespace Stress
{

static const tcu::Vec4 OCCLUDER_COLOR   = tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
static const tcu::Vec4 TARGET_COLOR     = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
static const int NUM_CASE_ITERATIONS    = 3;
static const int NUM_GENERATED_VERTICES = 100;
static const int WATCHDOG_INTERVAL      = 50; // Touch watchdog every N iterations.

class OcclusionQueryStressCase : public TestCase
{
public:
    OcclusionQueryStressCase(Context &ctx, const char *name, const char *desc, int m_numOccluderDraws,
                             int m_numOccludersPerDraw, int m_numTargetDraws, int m_numTargetsPerDraw, int m_numQueries,
                             uint32_t m_queryMode);
    ~OcclusionQueryStressCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    OcclusionQueryStressCase(const OcclusionQueryStressCase &);
    OcclusionQueryStressCase &operator=(const OcclusionQueryStressCase &);

    int m_numOccluderDraws;
    int m_numOccludersPerDraw;
    int m_numTargetDraws;
    int m_numTargetsPerDraw;
    int m_numQueries;
    uint32_t m_queryMode;

    glu::RenderContext &m_renderCtx;
    glu::ShaderProgram *m_program;
    int m_iterNdx;
    de::Random m_rnd;
};

OcclusionQueryStressCase::OcclusionQueryStressCase(Context &ctx, const char *name, const char *desc,
                                                   int numOccluderDraws, int numOccludersPerDraw, int numTargetDraws,
                                                   int numTargetsPerDraw, int numQueries, uint32_t queryMode)
    : TestCase(ctx, name, desc)
    , m_numOccluderDraws(numOccluderDraws)
    , m_numOccludersPerDraw(numOccludersPerDraw)
    , m_numTargetDraws(numTargetDraws)
    , m_numTargetsPerDraw(numTargetsPerDraw)
    , m_numQueries(numQueries)
    , m_queryMode(queryMode)
    , m_renderCtx(ctx.getRenderContext())
    , m_program(nullptr)
    , m_iterNdx(0)
    , m_rnd(deStringHash(name))
{
}

OcclusionQueryStressCase::~OcclusionQueryStressCase(void)
{
    OcclusionQueryStressCase::deinit();
}

void OcclusionQueryStressCase::init(void)
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

void OcclusionQueryStressCase::deinit(void)
{
    delete m_program;
    m_program = nullptr;
}

OcclusionQueryStressCase::IterateResult OcclusionQueryStressCase::iterate(void)
{
    tcu::TestLog &log  = m_testCtx.getLog();
    uint32_t colorUnif = glGetUniformLocation(m_program->getProgram(), "u_color");

    std::vector<float> vertices;
    std::vector<float> occluderVertices;
    std::vector<float> targetVertices;
    std::vector<uint32_t> queryIds(m_numQueries, 0);
    std::vector<uint32_t> queryResultReady(m_numQueries, 0);
    std::vector<uint32_t> queryResult(m_numQueries, 0);

    std::string sectionName("Case iteration " + de::toString(m_iterNdx + 1) + "/" + de::toString(NUM_CASE_ITERATIONS));
    tcu::ScopedLogSection section(log, sectionName.c_str(), sectionName.c_str());

    log << tcu::TestLog::Message << "Parameters:\n"
        << "- Number of occlusion queries: " << m_numQueries << ".\n"
        << "- Number of occluder draws per query: " << m_numOccluderDraws
        << ", primitives per draw: " << m_numOccludersPerDraw << ".\n"
        << "- Number of target draws per query: " << m_numTargetDraws
        << ", primitives per draw: " << m_numTargetsPerDraw << ".\n"
        << tcu::TestLog::EndMessage;

    int numOccluderIndicesPerDraw = 3 * m_numOccludersPerDraw;
    int numTargetIndicesPerDraw   = 3 * m_numTargetsPerDraw;

    // Generate vertex data

    vertices.resize(4 * NUM_GENERATED_VERTICES);

    for (int i = 0; i < NUM_GENERATED_VERTICES; i++)
    {
        vertices[4 * i]     = m_rnd.getFloat(-1.0f, 1.0f);
        vertices[4 * i + 1] = m_rnd.getFloat(-1.0f, 1.0f);
        vertices[4 * i + 2] = m_rnd.getFloat(0.0f, 1.0f);
        vertices[4 * i + 3] = 1.0f;
    }

    // Generate primitives

    occluderVertices.resize(4 * numOccluderIndicesPerDraw * m_numOccluderDraws);

    for (int i = 0; i < numOccluderIndicesPerDraw * m_numOccluderDraws; i++)
    {
        int vtxNdx                  = m_rnd.getInt(0, NUM_GENERATED_VERTICES - 1);
        occluderVertices[4 * i]     = vertices[4 * vtxNdx];
        occluderVertices[4 * i + 1] = vertices[4 * vtxNdx + 1];
        occluderVertices[4 * i + 2] = vertices[4 * vtxNdx + 2];
        occluderVertices[4 * i + 3] = vertices[4 * vtxNdx + 3];
    }

    targetVertices.resize(4 * numTargetIndicesPerDraw * m_numTargetDraws);

    for (int i = 0; i < numTargetIndicesPerDraw * m_numTargetDraws; i++)
    {
        int vtxNdx                = m_rnd.getInt(0, NUM_GENERATED_VERTICES - 1);
        targetVertices[4 * i]     = vertices[4 * vtxNdx];
        targetVertices[4 * i + 1] = vertices[4 * vtxNdx + 1];
        targetVertices[4 * i + 2] = vertices[4 * vtxNdx + 2];
        targetVertices[4 * i + 3] = vertices[4 * vtxNdx + 3];
    }

    TCU_CHECK(m_program);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(m_program->getProgram());
    glEnableVertexAttribArray(0);

    uint64_t time = deGetMicroseconds();

    for (int queryIter = 0; queryIter < m_numQueries; queryIter++)
    {
        // Draw occluders

        glUniform4f(colorUnif, OCCLUDER_COLOR.x(), OCCLUDER_COLOR.y(), OCCLUDER_COLOR.z(), OCCLUDER_COLOR.w());

        for (int drawIter = 0; drawIter < m_numOccluderDraws; drawIter++)
        {
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, &occluderVertices[drawIter * numOccluderIndicesPerDraw]);
            glDrawArrays(GL_TRIANGLES, 0, numOccluderIndicesPerDraw);
        }

        // Begin occlusion query

        glGenQueries(1, &queryIds[queryIter]);
        glBeginQuery(m_queryMode, queryIds[queryIter]);

        // Draw targets

        glUniform4f(colorUnif, TARGET_COLOR.x(), TARGET_COLOR.y(), TARGET_COLOR.z(), TARGET_COLOR.w());

        for (int drawIter = 0; drawIter < m_numTargetDraws; drawIter++)
        {
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, &targetVertices[drawIter * numTargetIndicesPerDraw]);
            glDrawArrays(GL_TRIANGLES, 0, numTargetIndicesPerDraw);
        }

        // End occlusion query

        glEndQuery(m_queryMode);

        if ((queryIter % WATCHDOG_INTERVAL) == 0 && m_testCtx.getWatchDog())
            qpWatchDog_touch(m_testCtx.getWatchDog());
    }

    glFinish();
    glDisable(GL_DEPTH_TEST);

    uint64_t dTime = deGetMicroseconds() - time;
    log << tcu::TestLog::Message << "Total duration: " << dTime / 1000 << " ms" << tcu::TestLog::EndMessage;

    // Get results

    for (int queryIter = 0; queryIter < m_numQueries; queryIter++)
    {
        glGetQueryObjectuiv(queryIds[queryIter], GL_QUERY_RESULT_AVAILABLE, &queryResultReady[queryIter]);

        if (queryResultReady[queryIter] == GL_TRUE)
        {
            glGetQueryObjectuiv(queryIds[queryIter], GL_QUERY_RESULT, &queryResult[queryIter]);
        }
        else
            TCU_FAIL("Occlusion query failed to return a result after glFinish()");

        if ((queryIter % WATCHDOG_INTERVAL) == 0 && m_testCtx.getWatchDog())
            qpWatchDog_touch(m_testCtx.getWatchDog());
    }

    glDeleteQueries(m_numQueries, &queryIds[0]);
    GLU_CHECK_MSG("Occlusion queries finished");

    log << tcu::TestLog::Message << "Case passed!" << tcu::TestLog::EndMessage;

    return (++m_iterNdx < NUM_CASE_ITERATIONS) ? CONTINUE : STOP;
}

OcclusionQueryTests::OcclusionQueryTests(Context &testCtx)
    : TestCaseGroup(testCtx, "occlusion_query", "Occlusion query stress tests")
{
}

OcclusionQueryTests::~OcclusionQueryTests(void)
{
}

void OcclusionQueryTests::init(void)
{
    addChild(new OcclusionQueryStressCase(m_context, "10_queries_2500_triangles_per_query",
                                          "10_queries_2500_triangles_per_query", 49, 50, 1, 50, 10,
                                          GL_ANY_SAMPLES_PASSED));
    addChild(new OcclusionQueryStressCase(m_context, "100_queries_2500_triangles_per_query",
                                          "100_queries_2500_triangles_per_query", 49, 50, 1, 50, 100,
                                          GL_ANY_SAMPLES_PASSED));
    addChild(new OcclusionQueryStressCase(m_context, "1000_queries_500_triangles_per_query",
                                          "1000_queries_500_triangles_per_query", 49, 10, 1, 10, 1000,
                                          GL_ANY_SAMPLES_PASSED));
    addChild(new OcclusionQueryStressCase(m_context, "10000_queries_20_triangles_per_query",
                                          "10000_queries_20_triangles_per_query", 1, 19, 1, 1, 10000,
                                          GL_ANY_SAMPLES_PASSED));
}

} // namespace Stress
} // namespace gles3
} // namespace deqp
