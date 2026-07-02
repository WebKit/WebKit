#ifndef _GLSSHADERPERFORMANCECASE_HPP
#define _GLSSHADERPERFORMANCECASE_HPP
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

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "glsShaderPerformanceMeasurer.hpp"
#include "deSharedPtr.hpp"

namespace deqp
{
namespace gls
{

class ShaderPerformanceCase : public tcu::TestCase
{
public:
    struct InitialCalibration
    {
        int initialNumCalls;
        InitialCalibration(void) : initialNumCalls(1)
        {
        }
    };

    ShaderPerformanceCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                          const char *description, PerfCaseType caseType);
    ~ShaderPerformanceCase(void);

    void setCalibrationInitialParamStorage(const de::SharedPtr<InitialCalibration> &storage)
    {
        m_initialCalibration = storage;
    }

    void init(void);
    void deinit(void);

    IterateResult iterate(void);

protected:
    virtual void setupProgram(uint32_t program);
    virtual void setupRenderState(void);

    void setGridSize(int gridW, int gridH);
    void setViewportSize(int width, int height);
    void setVertexFragmentRatio(float fragmentsPerVertices);

    int getGridWidth(void) const
    {
        return m_measurer.getGridWidth();
    }
    int getGridHeight(void) const
    {
        return m_measurer.getGridHeight();
    }
    int getViewportWidth(void) const
    {
        return m_measurer.getViewportWidth();
    }
    int getViewportHeight(void) const
    {
        return m_measurer.getViewportHeight();
    }

    virtual void reportResult(float mvertPerSecond, float mfragPerSecond);

    glu::RenderContext &m_renderCtx;

    PerfCaseType m_caseType;

    std::string m_vertShaderSource;
    std::string m_fragShaderSource;
    std::vector<AttribSpec> m_attributes;

private:
    glu::ShaderProgram *m_program;
    ShaderPerformanceMeasurer m_measurer;

    de::SharedPtr<InitialCalibration> m_initialCalibration;
};

class ShaderPerformanceCaseGroup : public tcu::TestCaseGroup
{
public:
    ShaderPerformanceCaseGroup(tcu::TestContext &testCtx, const char *name, const char *description);
    void addChild(ShaderPerformanceCase *);

private:
    de::SharedPtr<ShaderPerformanceCase::InitialCalibration> m_initialCalibrationStorage;
};

} // namespace gls
} // namespace deqp

#endif // _GLSSHADERPERFORMANCECASE_HPP
