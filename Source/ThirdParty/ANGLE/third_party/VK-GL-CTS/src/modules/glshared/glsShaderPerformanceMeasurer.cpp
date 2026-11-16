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
 * \brief Shader performance measurer; handles calibration and measurement
 *//*--------------------------------------------------------------------*/

#include "glsShaderPerformanceMeasurer.hpp"
#include "gluDefs.hpp"
#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"
#include "deStringUtil.hpp"
#include "deMath.h"
#include "deClock.h"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include <algorithm>

using std::string;
using std::vector;
using tcu::TestLog;
using tcu::Vec4;
using namespace glw; // GL types

namespace deqp
{
namespace gls
{

static inline float triangleInterpolate(float v0, float v1, float v2, float x, float y)
{
    return v0 + (v2 - v0) * x + (v1 - v0) * y;
}

static inline float triQuadInterpolate(float x, float y, const tcu::Vec4 &quad)
{
    // \note Top left fill rule.
    if (x + y < 1.0f)
        return triangleInterpolate(quad.x(), quad.y(), quad.z(), x, y);
    else
        return triangleInterpolate(quad.w(), quad.z(), quad.y(), 1.0f - x, 1.0f - y);
}

static inline int getNumVertices(int gridSizeX, int gridSizeY)
{
    return (gridSizeX + 1) * (gridSizeY + 1);
}

static inline int getNumIndices(int gridSizeX, int gridSizeY)
{
    return gridSizeX * gridSizeY * 6;
}

static inline uint16_t getVtxIndex(int x, int y, int gridSizeX)
{
    return (uint16_t)(y * (gridSizeX + 1) + x);
}

static void generateVertices(std::vector<float> &dst, int gridSizeX, int gridSizeY, const AttribSpec &spec)
{
    const int numComponents = 4;

    DE_ASSERT((gridSizeX + 1) * (gridSizeY + 1) <= (1 << 16)); // Must fit into 16-bit indices.
    DE_ASSERT(gridSizeX >= 1 && gridSizeY >= 1);
    dst.resize((gridSizeX + 1) * (gridSizeY + 1) * 4);

    for (int y = 0; y <= gridSizeY; y++)
    {
        for (int x = 0; x <= gridSizeX; x++)
        {
            float xf = (float)x / (float)gridSizeX;
            float yf = (float)y / (float)gridSizeY;

            for (int compNdx = 0; compNdx < numComponents; compNdx++)
                dst[getVtxIndex(x, y, gridSizeX) * numComponents + compNdx] = triQuadInterpolate(
                    xf, yf, tcu::Vec4(spec.p00[compNdx], spec.p01[compNdx], spec.p10[compNdx], spec.p11[compNdx]));
        }
    }
}

static void generateIndices(std::vector<uint16_t> &dst, int gridSizeX, int gridSizeY)
{
    const int numIndicesPerQuad = 6;
    int numIndices              = gridSizeX * gridSizeY * numIndicesPerQuad;
    dst.resize(numIndices);

    for (int y = 0; y < gridSizeY; y++)
    {
        for (int x = 0; x < gridSizeX; x++)
        {
            int quadNdx = y * gridSizeX + x;

            dst[quadNdx * numIndicesPerQuad + 0] = getVtxIndex(x + 0, y + 0, gridSizeX);
            dst[quadNdx * numIndicesPerQuad + 1] = getVtxIndex(x + 1, y + 0, gridSizeX);
            dst[quadNdx * numIndicesPerQuad + 2] = getVtxIndex(x + 0, y + 1, gridSizeX);

            dst[quadNdx * numIndicesPerQuad + 3] = getVtxIndex(x + 0, y + 1, gridSizeX);
            dst[quadNdx * numIndicesPerQuad + 4] = getVtxIndex(x + 1, y + 0, gridSizeX);
            dst[quadNdx * numIndicesPerQuad + 5] = getVtxIndex(x + 1, y + 1, gridSizeX);
        }
    }
}

ShaderPerformanceMeasurer::ShaderPerformanceMeasurer(const glu::RenderContext &renderCtx, PerfCaseType measureType)
    : m_renderCtx(renderCtx)
    , m_gridSizeX(measureType == CASETYPE_FRAGMENT ? 1 : 255)
    , m_gridSizeY(measureType == CASETYPE_FRAGMENT ? 1 : 255)
    , m_viewportWidth(measureType == CASETYPE_VERTEX ? 32 : renderCtx.getRenderTarget().getWidth())
    , m_viewportHeight(measureType == CASETYPE_VERTEX ? 32 : renderCtx.getRenderTarget().getHeight())
    , m_state(STATE_UNINITIALIZED)
    , m_isFirstIteration(false)
    , m_prevRenderStartTime(0)
    , m_result(-1.0f, -1.0f)
    , m_indexBuffer(0)
    , m_vao(0)
{
}

void ShaderPerformanceMeasurer::logParameters(TestLog &log) const
{
    log << TestLog::Message << "Grid size: " << m_gridSizeX << "x" << m_gridSizeY << TestLog::EndMessage
        << TestLog::Message << "Viewport: " << m_viewportWidth << "x" << m_viewportHeight << TestLog::EndMessage;
}

void ShaderPerformanceMeasurer::init(uint32_t program, const vector<AttribSpec> &attributes,
                                     int calibratorInitialNumCalls)
{
    DE_ASSERT(m_state == STATE_UNINITIALIZED);

    const glw::Functions &gl = m_renderCtx.getFunctions();
    const bool useVAO        = glu::isContextTypeGLCore(m_renderCtx.getType());

    if (useVAO)
    {
        DE_ASSERT(!m_vao);
        gl.genVertexArrays(1, &m_vao);
        gl.bindVertexArray(m_vao);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Create VAO");
    }

    // Validate that we have sane grid and viewport setup.

    DE_ASSERT(de::inBounds(m_gridSizeX, 1, 256) && de::inBounds(m_gridSizeY, 1, 256));

    {
        bool widthTooSmall  = m_renderCtx.getRenderTarget().getWidth() < m_viewportWidth;
        bool heightTooSmall = m_renderCtx.getRenderTarget().getHeight() < m_viewportHeight;

        if (widthTooSmall || heightTooSmall)
            throw tcu::NotSupportedError(
                "Render target too small (" +
                (widthTooSmall ? "width must be at least " + de::toString(m_viewportWidth) : "") +
                (heightTooSmall ?
                     string(widthTooSmall ? ", " : "") + "height must be at least " + de::toString(m_viewportHeight) :
                     "") +
                ")");
    }

    TCU_CHECK_INTERNAL(de::inRange(m_viewportWidth, 1, m_renderCtx.getRenderTarget().getWidth()) &&
                       de::inRange(m_viewportHeight, 1, m_renderCtx.getRenderTarget().getHeight()));

    // Insert a_position to attributes.
    m_attributes = attributes;
    m_attributes.push_back(AttribSpec("a_position", Vec4(-1.0f, -1.0f, 0.0f, 1.0f), Vec4(1.0f, -1.0f, 0.0f, 1.0f),
                                      Vec4(-1.0f, 1.0f, 0.0f, 1.0f), Vec4(1.0f, 1.0f, 0.0f, 1.0f)));

    // Generate indices.
    {
        std::vector<uint16_t> indices;
        generateIndices(indices, m_gridSizeX, m_gridSizeY);

        gl.genBuffers(1, &m_indexBuffer);
        gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
        gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(uint16_t)), &indices[0],
                      GL_STATIC_DRAW);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Upload index data");
    }

    // Generate vertices.
    m_attribBuffers.resize(m_attributes.size(), 0);
    gl.genBuffers((GLsizei)m_attribBuffers.size(), &m_attribBuffers[0]);

    for (int attribNdx = 0; attribNdx < (int)m_attributes.size(); attribNdx++)
    {
        std::vector<float> vertices;
        generateVertices(vertices, m_gridSizeX, m_gridSizeY, m_attributes[attribNdx]);

        gl.bindBuffer(GL_ARRAY_BUFFER, m_attribBuffers[attribNdx]);
        gl.bufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(float)), &vertices[0], GL_STATIC_DRAW);
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Upload vertex data");

    // Setup attribute bindings.
    for (int attribNdx = 0; attribNdx < (int)m_attributes.size(); attribNdx++)
    {
        int location = gl.getAttribLocation(program, m_attributes[attribNdx].name.c_str());

        if (location >= 0)
        {
            gl.enableVertexAttribArray(location);
            gl.bindBuffer(GL_ARRAY_BUFFER, m_attribBuffers[attribNdx]);
            gl.vertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Setup vertex attribute state");
    }

    gl.useProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");

    m_state            = STATE_MEASURING;
    m_isFirstIteration = true;

    m_calibrator.clear(CalibratorParameters(calibratorInitialNumCalls, 10 /* calibrate iteration frames */,
                                            2000.0f /* calibrate iteration shortcut threshold (ms) */,
                                            16 /* max calibrate iterations */, 1000.0f / 30.0f /* frame time (ms) */,
                                            1000.0f / 60.0f /* frame time cap (ms) */,
                                            1000.0f /* target measure duration (ms) */));
}

void ShaderPerformanceMeasurer::deinit(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    if (m_indexBuffer)
    {
        gl.deleteBuffers(1, &m_indexBuffer);
        m_indexBuffer = 0;
    }

    if (m_vao)
    {
        gl.deleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }

    if (!m_attribBuffers.empty())
    {
        gl.deleteBuffers((GLsizei)m_attribBuffers.size(), &m_attribBuffers[0]);
        m_attribBuffers.clear();
    }

    m_state = STATE_UNINITIALIZED;
}

void ShaderPerformanceMeasurer::render(int numDrawCalls)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    GLsizei numIndices       = (GLsizei)getNumIndices(m_gridSizeX, m_gridSizeY);

    gl.viewport(0, 0, m_viewportWidth, m_viewportHeight);

    for (int callNdx = 0; callNdx < numDrawCalls; callNdx++)
        gl.drawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, nullptr);
}

void ShaderPerformanceMeasurer::iterate(void)
{
    DE_ASSERT(m_state == STATE_MEASURING);

    uint64_t renderStartTime = deGetMicroseconds();
    render(m_calibrator.getCallCount()); // Always render. This gives more stable performance behavior.

    TheilSenCalibrator::State calibratorState = m_calibrator.getState();

    if (calibratorState == TheilSenCalibrator::STATE_RECOMPUTE_PARAMS)
    {
        m_calibrator.recomputeParameters();

        m_isFirstIteration    = true;
        m_prevRenderStartTime = renderStartTime;
    }
    else if (calibratorState == TheilSenCalibrator::STATE_MEASURE)
    {
        if (!m_isFirstIteration)
            m_calibrator.recordIteration(renderStartTime - m_prevRenderStartTime);

        m_isFirstIteration    = false;
        m_prevRenderStartTime = renderStartTime;
    }
    else
    {
        DE_ASSERT(calibratorState == TheilSenCalibrator::STATE_FINISHED);

        GLU_EXPECT_NO_ERROR(m_renderCtx.getFunctions().getError(), "End of rendering");

        const MeasureState &measureState = m_calibrator.getMeasureState();

        // Compute result.
        uint64_t totalTime    = measureState.getTotalTime();
        int numFrames         = (int)measureState.frameTimes.size();
        int64_t numQuadGrids  = measureState.numDrawCalls * numFrames;
        int64_t numPixels     = (int64_t)m_viewportWidth * (int64_t)m_viewportHeight * numQuadGrids;
        int64_t numVertices   = (int64_t)getNumVertices(m_gridSizeX, m_gridSizeY) * numQuadGrids;
        double mfragPerSecond = (double)numPixels / (double)totalTime;
        double mvertPerSecond = (double)numVertices / (double)totalTime;

        m_result = Result((float)mvertPerSecond, (float)mfragPerSecond);
        m_state  = STATE_FINISHED;
    }
}

void ShaderPerformanceMeasurer::logMeasurementInfo(TestLog &log) const
{
    DE_ASSERT(m_state == STATE_FINISHED);

    const MeasureState &measureState(m_calibrator.getMeasureState());

    // Compute totals.
    uint64_t totalTime     = measureState.getTotalTime();
    int numFrames          = (int)measureState.frameTimes.size();
    int64_t numQuadGrids   = measureState.numDrawCalls * numFrames;
    int64_t numPixels      = (int64_t)m_viewportWidth * (int64_t)m_viewportHeight * numQuadGrids;
    int64_t numVertices    = (int64_t)getNumVertices(m_gridSizeX, m_gridSizeY) * numQuadGrids;
    double mfragPerSecond  = (double)numPixels / (double)totalTime;
    double mvertPerSecond  = (double)numVertices / (double)totalTime;
    double framesPerSecond = (double)numFrames / ((double)totalTime / 1000000.0);

    logCalibrationInfo(log, m_calibrator);

    log << TestLog::Float("FramesPerSecond", "Frames per second in measurement", "Frames/s", QP_KEY_TAG_PERFORMANCE,
                          (float)framesPerSecond)
        << TestLog::Float("FragmentsPerVertices", "Vertex-fragment ratio", "Fragments/Vertices", QP_KEY_TAG_NONE,
                          (float)numPixels / (float)numVertices)
        << TestLog::Float("FragmentPerf", "Fragment performance", "MPix/s", QP_KEY_TAG_PERFORMANCE,
                          (float)mfragPerSecond)
        << TestLog::Float("VertexPerf", "Vertex performance", "MVert/s", QP_KEY_TAG_PERFORMANCE, (float)mvertPerSecond);
}

void ShaderPerformanceMeasurer::setGridSize(int gridW, int gridH)
{
    DE_ASSERT(m_state == STATE_UNINITIALIZED);
    DE_ASSERT(de::inBounds(gridW, 1, 256) && de::inBounds(gridH, 1, 256));
    m_gridSizeX = gridW;
    m_gridSizeY = gridH;
}

void ShaderPerformanceMeasurer::setViewportSize(int width, int height)
{
    DE_ASSERT(m_state == STATE_UNINITIALIZED);
    DE_ASSERT(de::inRange(width, 1, m_renderCtx.getRenderTarget().getWidth()) &&
              de::inRange(height, 1, m_renderCtx.getRenderTarget().getHeight()));
    m_viewportWidth  = width;
    m_viewportHeight = height;
}

} // namespace gls
} // namespace deqp
