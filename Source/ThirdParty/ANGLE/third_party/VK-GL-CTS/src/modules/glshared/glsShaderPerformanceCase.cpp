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
 * \brief Single-program test case wrapper for ShaderPerformanceMeasurer.
 *//*--------------------------------------------------------------------*/

#include "glsShaderPerformanceCase.hpp"
#include "tcuRenderTarget.hpp"
#include "deStringUtil.hpp"
#include "deMath.h"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

using tcu::TestLog;
using tcu::Vec4;
using namespace glw; // GL types

namespace deqp
{
namespace gls
{

ShaderPerformanceCase::ShaderPerformanceCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                             const char *description, PerfCaseType caseType)
    : tcu::TestCase(testCtx, tcu::NODETYPE_PERFORMANCE, name, description)
    , m_renderCtx(renderCtx)
    , m_caseType(caseType)
    , m_program(nullptr)
    , m_measurer(renderCtx, caseType)
{
}

ShaderPerformanceCase::~ShaderPerformanceCase(void)
{
    ShaderPerformanceCase::deinit();
}

void ShaderPerformanceCase::setGridSize(int gridW, int gridH)
{
    m_measurer.setGridSize(gridW, gridH);
}

void ShaderPerformanceCase::setViewportSize(int width, int height)
{
    m_measurer.setViewportSize(width, height);
}

void ShaderPerformanceCase::setVertexFragmentRatio(float fragmentsPerVertices)
{
    const float eps = 0.01f;
    int gridW       = 255;
    int gridH       = 255;
    int viewportW   = m_renderCtx.getRenderTarget().getWidth();
    int viewportH   = m_renderCtx.getRenderTarget().getHeight();

    for (int i = 0; i < 10; i++)
    {
        int numVert = (gridW + 1) * (gridH + 1);
        int numFrag = viewportW * viewportH;
        float ratio = (float)numFrag / (float)numVert;

        if (de::abs(ratio - fragmentsPerVertices) < eps)
            break;
        else if (ratio < fragmentsPerVertices)
        {
            // Not enough fragments.
            numVert = deRoundFloatToInt32((float)numFrag / fragmentsPerVertices);

            while ((gridW + 1) * (gridH + 1) > numVert)
            {
                if (gridW > gridH)
                    gridW -= 1;
                else
                    gridH -= 1;
            }
        }
        else
        {
            // Not enough vertices.
            numFrag = deRoundFloatToInt32((float)numVert * fragmentsPerVertices);

            while (viewportW * viewportH > numFrag)
            {
                if (viewportW > viewportH)
                    viewportW -= 1;
                else
                    viewportH -= 1;
            }
        }
    }

    float finalRatio = (float)(viewportW * viewportH) / (float)((gridW + 1) * (gridH + 1));
    m_testCtx.getLog() << TestLog::Message
                       << "Requested fragment/vertex-ratio: " << de::floatToString(fragmentsPerVertices, 2) << "\n"
                       << "Computed fragment/vertex-ratio: " << de::floatToString(finalRatio, 2) << TestLog::EndMessage;

    setGridSize(gridW, gridH);
    setViewportSize(viewportW, viewportH);
}

static void logRenderTargetInfo(TestLog &log, const tcu::RenderTarget &renderTarget)
{
    log << TestLog::Section("RenderTarget", "Render target") << TestLog::Message << "size: " << renderTarget.getWidth()
        << "x" << renderTarget.getHeight() << TestLog::EndMessage << TestLog::Message << "bits:"
        << " R" << renderTarget.getPixelFormat().redBits << " G" << renderTarget.getPixelFormat().greenBits << " B"
        << renderTarget.getPixelFormat().blueBits << " A" << renderTarget.getPixelFormat().alphaBits << " D"
        << renderTarget.getDepthBits() << " S" << renderTarget.getStencilBits() << TestLog::EndMessage;

    if (renderTarget.getNumSamples() != 0)
        log << TestLog::Message << renderTarget.getNumSamples() << "x MSAA" << TestLog::EndMessage;
    else
        log << TestLog::Message << "No MSAA" << TestLog::EndMessage;

    log << TestLog::EndSection;
}

void ShaderPerformanceCase::init(void)
{
    tcu::TestLog &log = m_testCtx.getLog();

    m_program = new glu::ShaderProgram(m_renderCtx, glu::makeVtxFragSources(m_vertShaderSource, m_fragShaderSource));

    if (m_program->isOk())
    {
        const int initialCallCount = m_initialCalibration ? m_initialCalibration->initialNumCalls : 1;
        logRenderTargetInfo(log, m_renderCtx.getRenderTarget());
        m_measurer.init(m_program->getProgram(), m_attributes, initialCallCount);
        m_measurer.logParameters(log);
        log << *m_program;
    }
    else
    {
        log << *m_program;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Compile failed");
        return; // Skip rest of init.
    }

    setupProgram(m_program->getProgram());
    setupRenderState();
}

void ShaderPerformanceCase::deinit(void)
{
    delete m_program;
    m_program = nullptr;

    m_measurer.deinit();
}

void ShaderPerformanceCase::setupProgram(uint32_t program)
{
    DE_UNREF(program);
}

void ShaderPerformanceCase::setupRenderState(void)
{
}

ShaderPerformanceCase::IterateResult ShaderPerformanceCase::iterate(void)
{
    DE_ASSERT(m_program);

    if (!m_program->isOk()) // This happens when compilation failed in init().
        return STOP;

    m_measurer.iterate();

    if (m_measurer.isFinished())
    {
        m_measurer.logMeasurementInfo(m_testCtx.getLog());

        if (m_initialCalibration)
            m_initialCalibration->initialNumCalls = de::max(1, m_measurer.getFinalCallCount());

        const ShaderPerformanceMeasurer::Result result = m_measurer.getResult();
        reportResult(result.megaVertPerSec, result.megaFragPerSec);
        return STOP;
    }
    else
        return CONTINUE;
}

void ShaderPerformanceCase::reportResult(float mvertPerSecond, float mfragPerSecond)
{
    float result = 0.0f;
    switch (m_caseType)
    {
    case CASETYPE_VERTEX:
        result = mvertPerSecond;
        break;
    case CASETYPE_FRAGMENT:
        result = mfragPerSecond;
        break;
    case CASETYPE_BALANCED:
        result = mfragPerSecond;
        break;
    default:
        DE_ASSERT(false);
    }

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::floatToString(result, 2).c_str());
}

ShaderPerformanceCaseGroup::ShaderPerformanceCaseGroup(tcu::TestContext &testCtx, const char *name,
                                                       const char *description)
    : TestCaseGroup(testCtx, name, description)
    , m_initialCalibrationStorage(new ShaderPerformanceCase::InitialCalibration)
{
}

void ShaderPerformanceCaseGroup::addChild(ShaderPerformanceCase *perfCase)
{
    perfCase->setCalibrationInitialParamStorage(m_initialCalibrationStorage);
    TestCaseGroup::addChild(perfCase);
}

} // namespace gls
} // namespace deqp
