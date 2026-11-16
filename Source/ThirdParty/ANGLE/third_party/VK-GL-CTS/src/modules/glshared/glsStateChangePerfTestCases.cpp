/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief State change performance tests.
 *//*--------------------------------------------------------------------*/

#include "glsStateChangePerfTestCases.hpp"

#include "tcuTestLog.hpp"

#include "gluDefs.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include "deStringUtil.hpp"

#include "deClock.h"

#include <vector>
#include <algorithm>

using std::string;
using std::vector;
using tcu::TestLog;
using namespace glw;

namespace deqp
{
namespace gls
{

namespace
{

struct ResultStats
{
    double median;
    double mean;
    double variance;

    uint64_t min;
    uint64_t max;
};

ResultStats calculateStats(const vector<uint64_t> &values)
{
    ResultStats result = {0.0, 0.0, 0.0, 0xFFFFFFFFFFFFFFFFu, 0};

    uint64_t sum = 0;

    for (int i = 0; i < (int)values.size(); i++)
        sum += values[i];

    result.mean = ((double)sum) / (double)values.size();

    for (int i = 0; i < (int)values.size(); i++)
    {
        const double val = (double)values[i];
        result.variance += (val - result.mean) * (val - result.mean);
    }

    result.variance /= (double)values.size();

    {
        const int n = (int)(values.size() / 2);

        vector<uint64_t> sortedValues = values;

        std::sort(sortedValues.begin(), sortedValues.end());

        result.median = (double)sortedValues[n];
    }

    for (int i = 0; i < (int)values.size(); i++)
    {
        result.min = std::min(result.min, values[i]);
        result.max = std::max(result.max, values[i]);
    }

    return result;
}

void genIndices(vector<GLushort> &indices, int triangleCount)
{
    indices.reserve(triangleCount * 3);

    for (int triangleNdx = 0; triangleNdx < triangleCount; triangleNdx++)
    {
        indices.push_back((GLushort)(triangleNdx * 3));
        indices.push_back((GLushort)(triangleNdx * 3 + 1));
        indices.push_back((GLushort)(triangleNdx * 3 + 2));
    }
}

void genCoords(vector<GLfloat> &coords, int triangleCount)
{
    coords.reserve(triangleCount * 3 * 2);

    for (int triangleNdx = 0; triangleNdx < triangleCount; triangleNdx++)
    {
        if ((triangleNdx % 2) == 0)
        {
            // CW
            coords.push_back(-1.0f);
            coords.push_back(-1.0f);

            coords.push_back(1.0f);
            coords.push_back(-1.0f);

            coords.push_back(1.0f);
            coords.push_back(1.0f);
        }
        else
        {
            // CCW
            coords.push_back(-1.0f);
            coords.push_back(-1.0f);

            coords.push_back(-1.0f);
            coords.push_back(1.0f);

            coords.push_back(1.0f);
            coords.push_back(1.0f);
        }
    }
}

void genTextureData(vector<uint8_t> &data, int width, int height)
{
    data.clear();
    data.reserve(width * height * 4);

    for (int x = 0; x < width; x++)
    {
        for (int y = 0; y < height; y++)
        {
            data.push_back((uint8_t)((255 * x) / width));
            data.push_back((uint8_t)((255 * y) / width));
            data.push_back((uint8_t)((255 * x * y) / (width * height)));
            data.push_back(255);
        }
    }
}

double calculateVariance(const vector<uint64_t> &values, double avg)
{
    double sum = 0.0;

    for (int valueNdx = 0; valueNdx < (int)values.size(); valueNdx++)
    {
        double value = (double)values[valueNdx];
        sum += (value - avg) * (value - avg);
    }

    return sum / (double)values.size();
}

uint64_t findMin(const vector<uint64_t> &values)
{
    uint64_t min = ~0ull;

    for (int valueNdx = 0; valueNdx < (int)values.size(); valueNdx++)
        min = std::min(values[valueNdx], min);

    return min;
}

uint64_t findMax(const vector<uint64_t> &values)
{
    uint64_t max = 0;

    for (int valueNdx = 0; valueNdx < (int)values.size(); valueNdx++)
        max = std::max(values[valueNdx], max);

    return max;
}

uint64_t findMedian(const vector<uint64_t> &v)
{
    vector<uint64_t> values = v;
    size_t n                = values.size() / 2;

    std::nth_element(values.begin(), values.begin() + n, values.end());

    return values[n];
}

} // namespace

StateChangePerformanceCase::StateChangePerformanceCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                       const char *name, const char *description, DrawType drawType,
                                                       int drawCallCount, int triangleCount)
    : tcu::TestCase(testCtx, tcu::NODETYPE_PERFORMANCE, name, description)
    , m_renderCtx(renderCtx)
    , m_drawType(drawType)
    , m_iterationCount(100)
    , m_callCount(drawCallCount)
    , m_triangleCount(triangleCount)
{
}

StateChangePerformanceCase::~StateChangePerformanceCase(void)
{
    StateChangePerformanceCase::deinit();
}

void StateChangePerformanceCase::init(void)
{
    if (m_drawType == DRAWTYPE_INDEXED_USER_PTR)
        genIndices(m_indices, m_triangleCount);
}

void StateChangePerformanceCase::requireIndexBuffers(int count)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    if ((int)m_indexBuffers.size() >= count)
        return;

    m_indexBuffers.reserve(count);

    vector<GLushort> indices;
    genIndices(indices, m_triangleCount);

    while ((int)m_indexBuffers.size() < count)
    {
        GLuint buffer;

        gl.genBuffers(1, &buffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers()");

        gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
        gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(GLushort)), &(indices[0]),
                      GL_STATIC_DRAW);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData()");
        gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");

        m_indexBuffers.push_back(buffer);
    }
}

void StateChangePerformanceCase::requireCoordBuffers(int count)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    if ((int)m_coordBuffers.size() >= count)
        return;

    m_coordBuffers.reserve(count);

    vector<GLfloat> coords;
    genCoords(coords, m_triangleCount);

    while ((int)m_coordBuffers.size() < count)
    {
        GLuint buffer;

        gl.genBuffers(1, &buffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers()");

        gl.bindBuffer(GL_ARRAY_BUFFER, buffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
        gl.bufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(coords.size() * sizeof(GLfloat)), &(coords[0]), GL_STATIC_DRAW);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData()");
        gl.bindBuffer(GL_ARRAY_BUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");

        m_coordBuffers.push_back(buffer);
    }
}

void StateChangePerformanceCase::requirePrograms(int count)
{
    if ((int)m_programs.size() >= count)
        return;

    m_programs.reserve(count);

    while ((int)m_programs.size() < count)
    {
        string vertexShaderSource = "attribute mediump vec2 a_coord;\n"
                                    "varying mediump vec2 v_texCoord;\n"
                                    "void main (void)\n"
                                    "{\n"
                                    "\tv_texCoord = vec2(0.5) + 0.5" +
                                    de::toString(m_programs.size()) +
                                    " * a_coord.xy;\n"
                                    "\tgl_Position = vec4(a_coord, 0.5, 1.0);\n"
                                    "}";

        string fragmentShaderSource = "uniform sampler2D u_sampler;\n"
                                      "varying mediump vec2 v_texCoord;\n"
                                      "void main (void)\n"
                                      "{\n"
                                      "\tgl_FragColor = vec4(1.0" +
                                      de::toString(m_programs.size()) +
                                      " * texture2D(u_sampler, v_texCoord).xyz, 1.0);\n"
                                      "}";

        glu::ShaderProgram *program =
            new glu::ShaderProgram(m_renderCtx, glu::ProgramSources() << glu::VertexSource(vertexShaderSource)
                                                                      << glu::FragmentSource(fragmentShaderSource));

        if (!program->isOk())
        {
            m_testCtx.getLog() << *program;
            delete program;
            TCU_FAIL("Compile failed");
        }

        m_programs.push_back(program);
    }
}

void StateChangePerformanceCase::requireTextures(int count)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    const int textureWidth  = 64;
    const int textureHeight = 64;

    if ((int)m_textures.size() >= count)
        return;

    m_textures.reserve(count);

    vector<uint8_t> textureData;
    genTextureData(textureData, textureWidth, textureHeight);

    DE_ASSERT(textureData.size() == textureWidth * textureHeight * 4);

    while ((int)m_textures.size() < count)
    {
        GLuint texture;

        gl.genTextures(1, &texture);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures()");

        gl.bindTexture(GL_TEXTURE_2D, texture);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

        gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      &(textureData[0]));
        GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D()");

        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri()");
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri()");
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri()");
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri()");

        gl.bindTexture(GL_TEXTURE_2D, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

        m_textures.push_back(texture);
    }
}

void StateChangePerformanceCase::requireFramebuffers(int count)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    if ((int)m_framebuffers.size() >= count)
        return;

    m_framebuffers.reserve(count);

    requireRenderbuffers(count);

    while ((int)m_framebuffers.size() < count)
    {
        GLuint framebuffer;

        gl.genFramebuffers(1, &framebuffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGenFramebuffers()");

        gl.bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindFramebuffer()");

        gl.bindRenderbuffer(GL_RENDERBUFFER, m_renderbuffers[m_framebuffers.size()]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindRenderbuffer()");

        gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                   m_renderbuffers[m_framebuffers.size()]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glFramebufferRenderbuffer()");

        gl.bindRenderbuffer(GL_RENDERBUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindRenderbuffer()");

        gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindFramebuffer()");

        m_framebuffers.push_back(framebuffer);
    }
}

void StateChangePerformanceCase::requireRenderbuffers(int count)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    if ((int)m_renderbuffers.size() >= count)
        return;

    m_renderbuffers.reserve(count);

    while ((int)m_renderbuffers.size() < count)
    {
        GLuint renderbuffer;

        gl.genRenderbuffers(1, &renderbuffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGenRenderbuffers()");

        gl.bindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindRenderbuffer()");

        gl.renderbufferStorage(GL_RENDERBUFFER, GL_RGB565, 24, 24);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glRenderbufferStorage()");

        gl.bindRenderbuffer(GL_RENDERBUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindRenderbuffer()");

        m_renderbuffers.push_back(renderbuffer);
    }
}

void StateChangePerformanceCase::requireSamplers(int count)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    if ((int)m_samplers.size() >= count)
        return;

    m_samplers.reserve(count);

    while ((int)m_samplers.size() < count)
    {
        GLuint sampler;
        gl.genSamplers(1, &sampler);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGenSamplers()");
        m_samplers.push_back(sampler);
    }
}

void StateChangePerformanceCase::requireVertexArrays(int count)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    if ((int)m_vertexArrays.size() >= count)
        return;

    m_vertexArrays.reserve(count);

    while ((int)m_vertexArrays.size() < count)
    {
        GLuint vertexArray;
        gl.genVertexArrays(1, &vertexArray);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGenVertexArrays()");
        m_vertexArrays.push_back(vertexArray);
    }
}

void StateChangePerformanceCase::deinit(void)
{
    m_indices.clear();
    m_interleavedResults.clear();
    m_batchedResults.clear();

    {
        const glw::Functions &gl = m_renderCtx.getFunctions();

        if (!m_indexBuffers.empty())
        {
            gl.deleteBuffers((GLsizei)m_indexBuffers.size(), &(m_indexBuffers[0]));
            m_indexBuffers.clear();
        }

        if (!m_coordBuffers.empty())
        {
            gl.deleteBuffers((GLsizei)m_coordBuffers.size(), &(m_coordBuffers[0]));
            m_coordBuffers.clear();
        }

        if (!m_textures.empty())
        {
            gl.deleteTextures((GLsizei)m_textures.size(), &(m_textures[0]));
            m_textures.clear();
        }

        if (!m_framebuffers.empty())
        {
            gl.deleteFramebuffers((GLsizei)m_framebuffers.size(), &(m_framebuffers[0]));
            m_framebuffers.clear();
        }

        if (!m_renderbuffers.empty())
        {
            gl.deleteRenderbuffers((GLsizei)m_renderbuffers.size(), &(m_renderbuffers[0]));
            m_renderbuffers.clear();
        }

        if (!m_samplers.empty())
        {
            gl.deleteSamplers((GLsizei)m_samplers.size(), &m_samplers[0]);
            m_samplers.clear();
        }

        if (!m_vertexArrays.empty())
        {
            gl.deleteVertexArrays((GLsizei)m_vertexArrays.size(), &m_vertexArrays[0]);
            m_vertexArrays.clear();
        }

        for (int programNdx = 0; programNdx < (int)m_programs.size(); programNdx++)
        {
            delete m_programs[programNdx];
            m_programs[programNdx] = NULL;
        }
        m_programs.clear();
    }
}

void StateChangePerformanceCase::logAndSetTestResult(void)
{
    TestLog &log = m_testCtx.getLog();

    ResultStats interleaved = calculateStats(m_interleavedResults);
    ResultStats batched     = calculateStats(m_batchedResults);

    log << TestLog::Message << "Interleaved mean: " << interleaved.mean << TestLog::EndMessage;
    log << TestLog::Message << "Interleaved median: " << interleaved.median << TestLog::EndMessage;
    log << TestLog::Message << "Interleaved variance: " << interleaved.variance << TestLog::EndMessage;
    log << TestLog::Message << "Interleaved min: " << interleaved.min << TestLog::EndMessage;
    log << TestLog::Message << "Interleaved max: " << interleaved.max << TestLog::EndMessage;

    log << TestLog::Message << "Batched mean: " << batched.mean << TestLog::EndMessage;
    log << TestLog::Message << "Batched median: " << batched.median << TestLog::EndMessage;
    log << TestLog::Message << "Batched variance: " << batched.variance << TestLog::EndMessage;
    log << TestLog::Message << "Batched min: " << batched.min << TestLog::EndMessage;
    log << TestLog::Message << "Batched max: " << batched.max << TestLog::EndMessage;

    log << TestLog::Message << "Batched/Interleaved mean ratio: " << (interleaved.mean / batched.mean)
        << TestLog::EndMessage;
    log << TestLog::Message << "Batched/Interleaved median ratio: " << (interleaved.median / batched.median)
        << TestLog::EndMessage;

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS,
                            de::floatToString((float)(((double)interleaved.median) / batched.median), 2).c_str());
}

tcu::TestCase::IterateResult StateChangePerformanceCase::iterate(void)
{
    if (m_interleavedResults.empty() && m_batchedResults.empty())
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Message << "Draw call count: " << m_callCount << TestLog::EndMessage;
        log << TestLog::Message << "Per call triangle count: " << m_triangleCount << TestLog::EndMessage;
    }

    // \note [mika] Interleave sampling to balance effects of powerstate etc.
    if ((int)m_interleavedResults.size() < m_iterationCount && m_batchedResults.size() >= m_interleavedResults.size())
    {
        const glw::Functions &gl = m_renderCtx.getFunctions();
        uint64_t resBeginUs      = 0;
        uint64_t resEndUs        = 0;

        setupInitialState(gl);
        gl.finish();
        GLU_EXPECT_NO_ERROR(gl.getError(), "glFinish()");

        // Render result
        resBeginUs = deGetMicroseconds();

        renderTest(gl);

        gl.finish();
        resEndUs = deGetMicroseconds();
        GLU_EXPECT_NO_ERROR(gl.getError(), "glFinish()");

        m_interleavedResults.push_back(resEndUs - resBeginUs);

        return CONTINUE;
    }
    else if ((int)m_batchedResults.size() < m_iterationCount)
    {
        const glw::Functions &gl = m_renderCtx.getFunctions();
        uint64_t refBeginUs      = 0;
        uint64_t refEndUs        = 0;

        setupInitialState(gl);
        gl.finish();
        GLU_EXPECT_NO_ERROR(gl.getError(), "glFinish()");

        // Render reference
        refBeginUs = deGetMicroseconds();

        renderReference(gl);

        gl.finish();
        refEndUs = deGetMicroseconds();
        GLU_EXPECT_NO_ERROR(gl.getError(), "glFinish()");

        m_batchedResults.push_back(refEndUs - refBeginUs);

        return CONTINUE;
    }
    else
    {
        logAndSetTestResult();
        return STOP;
    }
}

void StateChangePerformanceCase::callDraw(const glw::Functions &gl)
{
    switch (m_drawType)
    {
    case DRAWTYPE_NOT_INDEXED:
        gl.drawArrays(GL_TRIANGLES, 0, m_triangleCount * 3);
        break;
    case DRAWTYPE_INDEXED_USER_PTR:
        gl.drawElements(GL_TRIANGLES, m_triangleCount * 3, GL_UNSIGNED_SHORT, &m_indices[0]);
        break;
    case DRAWTYPE_INDEXED_BUFFER:
        gl.drawElements(GL_TRIANGLES, m_triangleCount * 3, GL_UNSIGNED_SHORT, NULL);
        break;
    default:
        DE_ASSERT(false);
    }
}

// StateChangeCallPerformanceCase

StateChangeCallPerformanceCase::StateChangeCallPerformanceCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                               const char *name, const char *description)
    : tcu::TestCase(testCtx, tcu::NODETYPE_PERFORMANCE, name, description)
    , m_renderCtx(renderCtx)
    , m_iterationCount(100)
    , m_callCount(1000)
{
}

StateChangeCallPerformanceCase::~StateChangeCallPerformanceCase(void)
{
}

void StateChangeCallPerformanceCase::executeTest(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    uint64_t beginTimeUs     = 0;
    uint64_t endTimeUs       = 0;

    beginTimeUs = deGetMicroseconds();

    execCalls(gl, (int)m_results.size(), m_callCount);

    endTimeUs = deGetMicroseconds();

    m_results.push_back(endTimeUs - beginTimeUs);
}

void StateChangeCallPerformanceCase::logTestCase(void)
{
    TestLog &log = m_testCtx.getLog();

    log << TestLog::Message << "Iteration count: " << m_iterationCount << TestLog::EndMessage;
    log << TestLog::Message << "Per iteration call count: " << m_callCount << TestLog::EndMessage;
}

double calculateAverage(const vector<uint64_t> &values)
{
    uint64_t sum = 0;

    for (int valueNdx = 0; valueNdx < (int)values.size(); valueNdx++)
        sum += values[valueNdx];

    return ((double)sum) / (double)values.size();
}

void StateChangeCallPerformanceCase::logAndSetTestResult(void)
{
    TestLog &log = m_testCtx.getLog();

    uint64_t minUs         = findMin(m_results);
    uint64_t maxUs         = findMax(m_results);
    uint64_t medianUs      = findMedian(m_results);
    double avgIterationUs  = calculateAverage(m_results);
    double avgCallUs       = avgIterationUs / m_callCount;
    double varIteration    = calculateVariance(m_results, avgIterationUs);
    double avgMedianCallUs = ((double)medianUs) / m_callCount;

    log << TestLog::Message << "Min iteration time: " << minUs << "us" << TestLog::EndMessage;
    log << TestLog::Message << "Max iteration time: " << maxUs << "us" << TestLog::EndMessage;
    log << TestLog::Message << "Average iteration time: " << avgIterationUs << "us" << TestLog::EndMessage;
    log << TestLog::Message << "Iteration variance time: " << varIteration << TestLog::EndMessage;
    log << TestLog::Message << "Median iteration time: " << medianUs << "us" << TestLog::EndMessage;
    log << TestLog::Message << "Average call time: " << avgCallUs << "us" << TestLog::EndMessage;
    log << TestLog::Message << "Average call time for median iteration: " << avgMedianCallUs << "us"
        << TestLog::EndMessage;

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::floatToString((float)avgMedianCallUs, 3).c_str());
}

tcu::TestCase::IterateResult StateChangeCallPerformanceCase::iterate(void)
{
    if (m_results.empty())
        logTestCase();

    if ((int)m_results.size() < m_iterationCount)
    {
        executeTest();
        GLU_EXPECT_NO_ERROR(m_renderCtx.getFunctions().getError(), "Unexpected error");
        return CONTINUE;
    }
    else
    {
        logAndSetTestResult();
        return STOP;
    }
}

} // namespace gls
} // namespace deqp
