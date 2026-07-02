#ifndef _GLSSHADERPERFORMANCEMEASURER_HPP
#define _GLSSHADERPERFORMANCEMEASURER_HPP
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

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuVector.hpp"
#include "gluRenderContext.hpp"
#include "glsCalibration.hpp"

namespace deqp
{
namespace gls
{

enum PerfCaseType
{
    CASETYPE_VERTEX = 0,
    CASETYPE_FRAGMENT,
    CASETYPE_BALANCED,

    CASETYPE_LAST
};

struct AttribSpec
{
    AttribSpec(const char *name_, const tcu::Vec4 &p00_, const tcu::Vec4 &p01_, const tcu::Vec4 &p10_,
               const tcu::Vec4 &p11_)
        : name(name_)
        , p00(p00_)
        , p01(p01_)
        , p10(p10_)
        , p11(p11_)
    {
    }

    AttribSpec(void)
    {
    }

    std::string name;
    tcu::Vec4 p00; //!< Bottom left.
    tcu::Vec4 p01; //!< Bottom right.
    tcu::Vec4 p10; //!< Top left.
    tcu::Vec4 p11; //!< Top right.
};

class ShaderPerformanceMeasurer
{
public:
    struct Result
    {
        float megaVertPerSec;
        float megaFragPerSec;

        Result(float megaVertPerSec_, float megaFragPerSec_)
            : megaVertPerSec(megaVertPerSec_)
            , megaFragPerSec(megaFragPerSec_)
        {
        }
    };

    ShaderPerformanceMeasurer(const glu::RenderContext &renderCtx, PerfCaseType measureType);
    ~ShaderPerformanceMeasurer(void)
    {
        deinit();
    }

    void init(uint32_t program, const std::vector<AttribSpec> &attributes, int calibratorInitialNumCalls);
    void deinit(void);
    void iterate(void);

    void logParameters(tcu::TestLog &log) const;
    bool isFinished(void) const
    {
        return m_state == STATE_FINISHED;
    }
    Result getResult(void) const
    {
        DE_ASSERT(m_state == STATE_FINISHED);
        return m_result;
    }
    void logMeasurementInfo(tcu::TestLog &log) const;

    void setGridSize(int gridW, int gridH);
    void setViewportSize(int width, int height);

    int getGridWidth(void) const
    {
        return m_gridSizeX;
    }
    int getGridHeight(void) const
    {
        return m_gridSizeY;
    }
    int getViewportWidth(void) const
    {
        return m_viewportWidth;
    }
    int getViewportHeight(void) const
    {
        return m_viewportHeight;
    }

    int getFinalCallCount(void) const
    {
        DE_ASSERT(m_state == STATE_FINISHED);
        return m_calibrator.getCallCount();
    }

private:
    enum State
    {
        STATE_UNINITIALIZED = 0,
        STATE_MEASURING,
        STATE_FINISHED,

        STATE_LAST
    };

    void render(int numDrawCalls);

    const glu::RenderContext &m_renderCtx;
    int m_gridSizeX;
    int m_gridSizeY;
    int m_viewportWidth;
    int m_viewportHeight;

    State m_state;
    bool m_isFirstIteration;
    uint64_t m_prevRenderStartTime;
    Result m_result;
    TheilSenCalibrator m_calibrator;
    uint32_t m_indexBuffer;
    std::vector<AttribSpec> m_attributes;
    std::vector<uint32_t> m_attribBuffers;
    uint32_t m_vao;
};

} // namespace gls
} // namespace deqp

#endif // _GLSSHADERPERFORMANCEMEASURER_HPP
