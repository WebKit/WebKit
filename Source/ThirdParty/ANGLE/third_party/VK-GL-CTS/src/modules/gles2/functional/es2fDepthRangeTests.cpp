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
 * \brief glDepthRangef() tests.
 *//*--------------------------------------------------------------------*/

#include "es2fDepthRangeTests.hpp"
#include "tcuVector.hpp"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRenderTarget.hpp"
#include "gluPixelTransfer.hpp"
#include "gluShaderProgram.hpp"
#include "gluRenderContext.hpp"
#include "deRandom.hpp"
#include "deMath.h"
#include "deString.h"

#include "glw.h"

namespace deqp
{
namespace gles2
{
namespace Functional
{

enum
{
    VISUALIZE_DEPTH_STEPS = 32 //!< Number of depth steps in visualization
};

using std::string;
using std::vector;
using tcu::TestLog;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

static const char *s_vertexShaderSrc   = "attribute highp vec4 a_position;\n"
                                         "attribute highp vec2 a_coord;\n"
                                         "void main (void)\n"
                                         "{\n"
                                         "    gl_Position = a_position;\n"
                                         "}\n";
static const char *s_fragmentShaderSrc = "uniform mediump vec4 u_color;\n"
                                         "void main (void)\n"
                                         "{\n"
                                         "    gl_FragColor = u_color;\n"
                                         "}\n";

template <typename T>
static inline bool compare(uint32_t func, T a, T b)
{
    switch (func)
    {
    case GL_NEVER:
        return false;
    case GL_ALWAYS:
        return true;
    case GL_LESS:
        return a < b;
    case GL_LEQUAL:
        return a <= b;
    case GL_EQUAL:
        return a == b;
    case GL_NOTEQUAL:
        return a != b;
    case GL_GEQUAL:
        return a >= b;
    case GL_GREATER:
        return a > b;
    default:
        DE_ASSERT(false);
        return false;
    }
}

inline float triangleInterpolate(const float v0, const float v1, const float v2, const float x, const float y)
{
    return v0 + (v2 - v0) * x + (v1 - v0) * y;
}

inline float triQuadInterpolate(const float x, const float y, const tcu::Vec4 &quad)
{
    // \note Top left fill rule.
    if (x + y < 1.0f)
        return triangleInterpolate(quad.x(), quad.y(), quad.z(), x, y);
    else
        return triangleInterpolate(quad.w(), quad.z(), quad.y(), 1.0f - x, 1.0f - y);
}

inline float depthRangeTransform(const float zd, const float zNear, const float zFar)
{
    const float cNear = de::clamp(zNear, 0.0f, 1.0f);
    const float cFar  = de::clamp(zFar, 0.0f, 1.0f);
    return ((cFar - cNear) / 2.0f) * zd + (cNear + cFar) / 2.0f;
}

class DepthRangeCompareCase : public TestCase
{
public:
    DepthRangeCompareCase(Context &context, const char *name, const char *desc, const tcu::Vec4 &depthCoord,
                          const float zNear, const float zFar, const uint32_t compareFunc);
    ~DepthRangeCompareCase(void);

    IterateResult iterate(void);

private:
    const tcu::Vec4 m_depthCoord;
    const float m_zNear;
    const float m_zFar;
    const uint32_t m_compareFunc;
};

DepthRangeCompareCase::DepthRangeCompareCase(Context &context, const char *name, const char *desc,
                                             const tcu::Vec4 &depthCoord, const float zNear, const float zFar,
                                             const uint32_t compareFunc)
    : TestCase(context, name, desc)
    , m_depthCoord(depthCoord)
    , m_zNear(zNear)
    , m_zFar(zFar)
    , m_compareFunc(compareFunc)
{
}

DepthRangeCompareCase::~DepthRangeCompareCase(void)
{
}

DepthRangeCompareCase::IterateResult DepthRangeCompareCase::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    de::Random rnd(deStringHash(getName()));
    const tcu::RenderTarget &renderTarget = m_context.getRenderContext().getRenderTarget();
    const int viewportW                   = de::min(128, renderTarget.getWidth());
    const int viewportH                   = de::min(128, renderTarget.getHeight());
    const int viewportX                   = rnd.getInt(0, renderTarget.getWidth() - viewportW);
    const int viewportY                   = rnd.getInt(0, renderTarget.getHeight() - viewportH);
    tcu::Surface renderedFrame(viewportW, viewportH);
    tcu::Surface referenceFrame(viewportW, viewportH);
    const float constDepth = 0.1f;

    if (renderTarget.getDepthBits() == 0)
        throw tcu::NotSupportedError("Depth buffer is required", "", __FILE__, __LINE__);

    const glu::ShaderProgram program(m_context.getRenderContext(),
                                     glu::makeVtxFragSources(s_vertexShaderSrc, s_fragmentShaderSrc));

    if (!program.isOk())
    {
        log << program;
        TCU_FAIL("Compile failed");
    }

    const int colorLoc = glGetUniformLocation(program.getProgram(), "u_color");
    const int posLoc   = glGetAttribLocation(program.getProgram(), "a_position");

    m_testCtx.getLog() << TestLog::Message << "glDepthRangef(" << m_zNear << ", " << m_zFar << ")"
                       << TestLog::EndMessage;

    glViewport(viewportX, viewportY, viewportW, viewportH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(program.getProgram());
    glEnableVertexAttribArray(posLoc);

    static const uint16_t quadIndices[] = {0, 1, 2, 2, 1, 3};

    // Fill viewport with 2 quads - one with constant depth and another with d = [-1..1]
    {
        static const float constDepthCoord[]   = {-1.0f, -1.0f, constDepth, 1.0f, -1.0f, +1.0f, constDepth, 1.0f,
                                                  0.0f,  -1.0f, constDepth, 1.0f, 0.0f,  +1.0f, constDepth, 1.0f};
        static const float varyingDepthCoord[] = {0.0f,  -1.0f, +1.0f, 1.0f, 0.0f,  +1.0f, 0.0f,  1.0f,
                                                  +1.0f, -1.0f, 0.0f,  1.0f, +1.0f, +1.0f, -1.0f, 1.0f};

        glUniform4f(colorLoc, 0.0f, 0.0f, 1.0f, 1.0f);
        glDepthFunc(GL_ALWAYS);

        glVertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, &constDepthCoord);
        glDrawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(quadIndices), GL_UNSIGNED_SHORT, &quadIndices[0]);

        glVertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, &varyingDepthCoord);
        glDrawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(quadIndices), GL_UNSIGNED_SHORT, &quadIndices[0]);

        GLU_CHECK();
    }

    // Render with depth test.
    {
        const float position[] = {-1.0f, -1.0f, m_depthCoord[0], 1.0f, -1.0f, +1.0f, m_depthCoord[1], 1.0f,
                                  +1.0f, -1.0f, m_depthCoord[2], 1.0f, +1.0f, +1.0f, m_depthCoord[3], 1.0f};

        glDepthRangef(m_zNear, m_zFar);
        glDepthFunc(m_compareFunc);
        glUniform4f(colorLoc, 0.0f, 1.0f, 0.0f, 1.0f);

        glVertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, &position[0]);
        glDrawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(quadIndices), GL_UNSIGNED_SHORT, &quadIndices[0]);

        GLU_CHECK();
    }

    glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, renderedFrame.getAccess());

    // Render reference.
    for (int y = 0; y < referenceFrame.getHeight(); y++)
    {
        float yf = ((float)y + 0.5f) / (float)referenceFrame.getHeight();
        int half = de::clamp((int)((float)referenceFrame.getWidth() * 0.5f + 0.5f), 0, referenceFrame.getWidth());

        // Fill left half - comparison to constant 0.5
        for (int x = 0; x < half; x++)
        {
            float xf   = ((float)x + 0.5f) / (float)referenceFrame.getWidth();
            float d    = depthRangeTransform(triQuadInterpolate(xf, yf, m_depthCoord), m_zNear, m_zFar);
            bool dpass = compare(m_compareFunc, d, constDepth * 0.5f + 0.5f);

            referenceFrame.setPixel(x, y, dpass ? tcu::RGBA::green() : tcu::RGBA::blue());
        }

        // Fill right half - comparison to interpolated depth
        for (int x = half; x < referenceFrame.getWidth(); x++)
        {
            float xf   = ((float)x + 0.5f) / (float)referenceFrame.getWidth();
            float xh   = ((float)(x - half) + 0.5f) / (float)(referenceFrame.getWidth() - half);
            float rd   = 1.0f - (xh + yf) * 0.5f;
            float d    = depthRangeTransform(triQuadInterpolate(xf, yf, m_depthCoord), m_zNear, m_zFar);
            bool dpass = compare(m_compareFunc, d, rd);

            referenceFrame.setPixel(x, y, dpass ? tcu::RGBA::green() : tcu::RGBA::blue());
        }
    }

    bool isOk = tcu::fuzzyCompare(log, "Result", "Image comparison result", referenceFrame, renderedFrame, 0.05f,
                                  tcu::COMPARE_LOG_RESULT);
    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL, isOk ? "Pass" : "Fail");
    return STOP;
}

class DepthRangeWriteCase : public TestCase
{
public:
    DepthRangeWriteCase(Context &context, const char *name, const char *desc, const tcu::Vec4 &depthCoord,
                        const float zNear, const float zFar);
    ~DepthRangeWriteCase(void);

    IterateResult iterate(void);

private:
    const tcu::Vec4 &m_depthCoord;
    const float m_zNear;
    const float m_zFar;
};

DepthRangeWriteCase::DepthRangeWriteCase(Context &context, const char *name, const char *desc,
                                         const tcu::Vec4 &depthCoord, const float zNear, const float zFar)
    : TestCase(context, name, desc)
    , m_depthCoord(depthCoord)
    , m_zNear(zNear)
    , m_zFar(zFar)
{
}

DepthRangeWriteCase::~DepthRangeWriteCase(void)
{
}

DepthRangeWriteCase::IterateResult DepthRangeWriteCase::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    de::Random rnd(deStringHash(getName()));
    const tcu::RenderTarget &renderTarget = m_context.getRenderContext().getRenderTarget();
    const int viewportW                   = de::min(128, renderTarget.getWidth());
    const int viewportH                   = de::min(128, renderTarget.getHeight());
    const int viewportX                   = rnd.getInt(0, renderTarget.getWidth() - viewportW);
    const int viewportY                   = rnd.getInt(0, renderTarget.getHeight() - viewportH);
    tcu::Surface renderedFrame(viewportW, viewportH);
    tcu::Surface referenceFrame(viewportW, viewportH);
    const int numDepthSteps = VISUALIZE_DEPTH_STEPS;
    const float depthStep   = 1.0f / (float)(numDepthSteps - 1);

    if (renderTarget.getDepthBits() == 0)
        throw tcu::NotSupportedError("Depth buffer is required", "", __FILE__, __LINE__);

    const glu::ShaderProgram program(m_context.getRenderContext(),
                                     glu::makeVtxFragSources(s_vertexShaderSrc, s_fragmentShaderSrc));

    if (!program.isOk())
    {
        log << program;
        TCU_FAIL("Compile failed");
    }

    const int colorLoc = glGetUniformLocation(program.getProgram(), "u_color");
    const int posLoc   = glGetAttribLocation(program.getProgram(), "a_position");

    m_testCtx.getLog() << TestLog::Message << "glDepthRangef(" << m_zNear << ", " << m_zFar << ")"
                       << TestLog::EndMessage;

    glViewport(viewportX, viewportY, viewportW, viewportH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(program.getProgram());
    glEnableVertexAttribArray(posLoc);

    static const uint16_t quadIndices[] = {0, 1, 2, 2, 1, 3};

    // Render with depth range.
    {
        const float position[] = {-1.0f, -1.0f, m_depthCoord[0], 1.0f, -1.0f, +1.0f, m_depthCoord[1], 1.0f,
                                  +1.0f, -1.0f, m_depthCoord[2], 1.0f, +1.0f, +1.0f, m_depthCoord[3], 1.0f};

        glDepthFunc(GL_ALWAYS);
        glDepthRangef(m_zNear, m_zFar);
        glUniform4f(colorLoc, 0.0f, 1.0f, 0.0f, 1.0f);
        glVertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, &position[0]);
        glDrawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(quadIndices), GL_UNSIGNED_SHORT, &quadIndices[0]);
        GLU_CHECK();
    }

    // Visualize by rendering full-screen quads with increasing depth and color.
    {
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glDepthRangef(0.0f, 1.0f);

        for (int stepNdx = 0; stepNdx < numDepthSteps; stepNdx++)
        {
            float f     = (float)stepNdx * depthStep;
            float depth = f * 2.0f - 1.0f;
            Vec4 color  = Vec4(f, f, f, 1.0f);

            float position[] = {-1.0f, -1.0f, depth, 1.0f, -1.0f, +1.0f, depth, 1.0f,
                                +1.0f, -1.0f, depth, 1.0f, +1.0f, +1.0f, depth, 1.0f};

            glUniform4fv(colorLoc, 1, color.getPtr());
            glVertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, &position[0]);
            glDrawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(quadIndices), GL_UNSIGNED_SHORT, &quadIndices[0]);
        }

        GLU_CHECK();
    }

    glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, renderedFrame.getAccess());

    // Render reference.
    for (int y = 0; y < referenceFrame.getHeight(); y++)
    {
        for (int x = 0; x < referenceFrame.getWidth(); x++)
        {
            float xf = ((float)x + 0.5f) / (float)referenceFrame.getWidth();
            float yf = ((float)y + 0.5f) / (float)referenceFrame.getHeight();
            float d  = depthRangeTransform(triQuadInterpolate(xf, yf, m_depthCoord), m_zNear, m_zFar);
            int step = (int)deFloatFloor(d / depthStep);
            int col  = de::clamp(deRoundFloatToInt32((float)step * depthStep * 255.0f), 0, 255);

            referenceFrame.setPixel(x, y, tcu::RGBA(col, col, col, 0xff));
        }
    }

    bool isOk = tcu::fuzzyCompare(log, "Result", "Image comparison result", referenceFrame, renderedFrame, 0.05f,
                                  tcu::COMPARE_LOG_RESULT);
    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL, isOk ? "Pass" : "Fail");
    return STOP;
}

DepthRangeTests::DepthRangeTests(Context &context) : TestCaseGroup(context, "depth_range", "glDepthRangef() tests")
{
}

DepthRangeTests::~DepthRangeTests(void)
{
}

void DepthRangeTests::init(void)
{
    static const struct
    {
        const char *name;
        const char *desc;
        const tcu::Vec4 depthCoord;
        const float zNear;
        const float zFar;
    } cases[] = {{"default", "Default depth range", tcu::Vec4(-1.0f, 0.2f, -0.3f, 1.0f), 0.0f, 1.0f},
                 {"reverse", "Reversed default range", tcu::Vec4(-1.0f, 0.2f, -0.3f, 1.0f), 1.0f, 0.0f},
                 {"zero_to_half", "From 0 to 0.5", tcu::Vec4(-1.0f, 0.2f, -0.3f, 1.0f), 0.0f, 0.5f},
                 {"half_to_one", "From 0.5 to 1", tcu::Vec4(-1.0f, 0.2f, -0.3f, 1.0f), 0.5f, 1.0f},
                 {"half_to_zero", "From 0.5 to 0", tcu::Vec4(-1.0f, 0.2f, -0.3f, 1.0f), 0.5f, 0.0f},
                 {"one_to_half", "From 1 to 0.5", tcu::Vec4(-1.0f, 0.2f, -0.3f, 1.0f), 1.0f, 0.5f},
                 {"third_to_0_8", "From 1/3 to 0.8", tcu::Vec4(-1.0f, 0.2f, -0.3f, 1.0f), 1.0f / 3.0f, 0.8f},
                 {"0_8_to_third", "From 0.8 to 1/3", tcu::Vec4(-1.0f, 0.2f, -0.3f, 1.0f), 0.8f, 1.0f / 3.0f},
                 {"zero_to_zero", "From 0 to 0", tcu::Vec4(-1.0f, 0.2f, -0.3f, 1.0f), 0.0f, 0.0f},
                 {"half_to_half", "From 0.5 to 0.5", tcu::Vec4(-1.0f, 0.2f, -0.3f, 1.0f), 0.5f, 0.5f},
                 {"one_to_one", "From 1 to 1", tcu::Vec4(-1.0f, 0.2f, -0.3f, 1.0f), 1.0f, 1.0f},
                 {"clamp_near", "From -1 to 1", tcu::Vec4(-1.0f, 0.2f, -0.3f, 1.0f), -1.0f, 1.0f},
                 {"clamp_far", "From 0 to 2", tcu::Vec4(-1.0f, 0.2f, -0.3f, 1.0f), 0.0f, 2.0},
                 {"clamp_both", "From -1 to 2", tcu::Vec4(-1.0f, 0.2f, -0.3f, 1.0f), -1.0, 2.0}};

    // .write
    tcu::TestCaseGroup *writeGroup = new tcu::TestCaseGroup(m_testCtx, "write", "gl_FragDepth write tests");
    addChild(writeGroup);
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(cases); ndx++)
        writeGroup->addChild(new DepthRangeWriteCase(m_context, cases[ndx].name, cases[ndx].desc, cases[ndx].depthCoord,
                                                     cases[ndx].zNear, cases[ndx].zFar));

    // .compare
    tcu::TestCaseGroup *compareGroup =
        new tcu::TestCaseGroup(m_testCtx, "compare", "gl_FragDepth used with depth comparison");
    addChild(compareGroup);
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(cases); ndx++)
        compareGroup->addChild(new DepthRangeCompareCase(m_context, cases[ndx].name, cases[ndx].desc,
                                                         cases[ndx].depthCoord, cases[ndx].zNear, cases[ndx].zFar,
                                                         GL_LESS));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
