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
 * \brief Long running shader stress tests.
 *//*--------------------------------------------------------------------*/

#include "es3sLongRunningShaderTests.hpp"

#include "gluShaderProgram.hpp"
#include "gluShaderUtil.hpp"
#include "gluDrawUtil.hpp"

#include "tcuRenderTarget.hpp"
#include "tcuVector.hpp"
#include "tcuTestLog.hpp"

#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deString.h"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles3
{
namespace Stress
{

using std::vector;
using tcu::TestLog;
using tcu::Vec2;

namespace
{

enum LoopType
{
    LOOPTYPE_FOR = 0,
    LOOPTYPE_WHILE,
    LOOPTYPE_DO_WHILE,

    LOOPTYPE_LAST
};

enum IterCountType
{
    ITERCOUNTTYPE_STATIC = 0,
    ITERCOUNTTYPE_UNIFORM,
    ITERCOUNTTYPE_DYNAMIC,

    ITERCOUNTTYPE_LAST
};

class LongRunningShaderCase : public TestCase
{
public:
    struct Params
    {
        const char *name;
        const char *description;
        glu::ShaderType shaderType;
        LoopType loopType;
        IterCountType iterCountType;
        int numInvocations;
        int minLoopIterCount;
        int maxLoopIterCount;
    };

    LongRunningShaderCase(Context &context, const Params *params);
    ~LongRunningShaderCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    LongRunningShaderCase(const LongRunningShaderCase &);
    LongRunningShaderCase &operator=(const LongRunningShaderCase &);

    static glu::ProgramSources genSources(const Params &params);
    static uint32_t getSeed(const Params &params);

    const Params *const m_params;
    const int m_numCaseIters;

    glu::ShaderProgram *m_program;
    int m_caseIterNdx;
};

LongRunningShaderCase::LongRunningShaderCase(Context &context, const Params *params)
    : TestCase(context, params->name, params->description)
    , m_params(params)
    , m_numCaseIters(5)
    , m_program(nullptr)
    , m_caseIterNdx(0)
{
}

LongRunningShaderCase::~LongRunningShaderCase(void)
{
    deinit();
}

glu::ProgramSources LongRunningShaderCase::genSources(const Params &params)
{
    const bool isVertCase = params.shaderType == glu::SHADERTYPE_VERTEX;
    std::ostringstream vert, frag;

    vert << "#version 300 es\n"
         << "in highp vec2 a_position;\n";

    frag << "#version 300 es\n";

    if (params.iterCountType == ITERCOUNTTYPE_DYNAMIC)
    {
        vert << "in highp int a_iterCount;\n";
        if (!isVertCase)
        {
            vert << "flat out highp int v_iterCount;\n";
            frag << "flat in highp int v_iterCount;\n";
        }
    }
    else if (params.iterCountType == ITERCOUNTTYPE_UNIFORM)
        (isVertCase ? vert : frag) << "uniform highp int u_iterCount;\n";

    if (isVertCase)
    {
        vert << "out mediump vec4 v_color;\n";
        frag << "in mediump vec4 v_color;\n";
    }

    frag << "out mediump vec4 o_color;\n";

    vert << "\nvoid main (void)\n{\n"
         << "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
         << "    gl_PointSize = 1.0;\n";

    if (!isVertCase && params.iterCountType == ITERCOUNTTYPE_DYNAMIC)
        vert << "    v_iterCount = a_iterCount;\n";

    frag << "\nvoid main (void)\n{\n";

    {
        const std::string iterCount =
            params.iterCountType == ITERCOUNTTYPE_DYNAMIC ? (isVertCase ? "a_iterCount" : "v_iterCount") :
            params.iterCountType == ITERCOUNTTYPE_UNIFORM ? "u_iterCount" :
            params.iterCountType == ITERCOUNTTYPE_STATIC  ? de::toString(params.maxLoopIterCount) :
                                                            "<invalid>";
        const char *const body = "color = cos(sin(color*1.25)*0.8);";
        std::ostringstream &op = isVertCase ? vert : frag;

        op << "    mediump vec4 color = " << (isVertCase ? "a_position.xyxy" : "gl_FragCoord") << ";\n";

        if (params.loopType == LOOPTYPE_FOR)
        {
            op << "    for (highp int i = 0; i < " << iterCount << " || " << iterCount << " < 0; ++i)\n"
               << "        " << body << "\n";
        }
        else if (params.loopType == LOOPTYPE_WHILE)
        {
            op << "    highp int i = 0;\n"
               << "    while (i < " << iterCount << " || " << iterCount << " < 0) {\n"
               << "        i += 1;\n"
               << "        " << body << "\n"
               << "    }\n";
        }
        else
        {
            DE_ASSERT(params.loopType == LOOPTYPE_DO_WHILE);
            op << "    highp int i = 0;\n"
               << "    do {\n"
               << "        i += 1;\n"
               << "        " << body << "\n"
               << "    } while (i <= " << iterCount << " || " << iterCount << " < 0);\n";
        }
    }

    if (isVertCase)
    {
        vert << "    v_color = color;\n";
        frag << "    o_color = v_color;\n";
    }
    else
        frag << "    o_color = color;\n";

    vert << "}\n";
    frag << "}\n";

    return glu::ProgramSources() << glu::VertexSource(vert.str()) << glu::FragmentSource(frag.str());
}

void LongRunningShaderCase::init(void)
{
    DE_ASSERT(!m_program);
    m_program = new glu::ShaderProgram(m_context.getRenderContext(), genSources(*m_params));

    m_testCtx.getLog() << *m_program;

    if (!m_program->isOk())
    {
        deinit();
        TCU_FAIL("Failed to compile shader program");
    }

    m_caseIterNdx = 0;

    if (m_params->iterCountType != ITERCOUNTTYPE_STATIC)
    {
        m_testCtx.getLog() << TestLog::Message << "Loop iteration counts in range: [" << m_params->minLoopIterCount
                           << ", " << m_params->maxLoopIterCount << "]" << TestLog::EndMessage;
    }

    m_testCtx.getLog() << TestLog::Message << "Number of vertices and fragments: " << m_params->numInvocations
                       << TestLog::EndMessage;

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass"); // Test will pass or timeout, unless driver/device crashes.
}

void LongRunningShaderCase::deinit(void)
{
    delete m_program;
    m_program = nullptr;
}

void genPositions(const tcu::RenderTarget &renderTarget, int numPoints, Vec2 *positions)
{
    const int width  = renderTarget.getWidth();
    const int height = renderTarget.getHeight();

    if (width * height < numPoints)
        throw tcu::NotSupportedError("Too small viewport to fit all test points");

    for (int pointNdx = 0; pointNdx < numPoints; pointNdx++)
    {
        const int xi   = pointNdx % width;
        const int yi   = pointNdx / height;
        const float xf = 2.0f * ((float(xi) + 0.5f) / float(width)) - 1.0f;
        const float yf = 2.0f * ((float(yi) + 0.5f) / float(height)) - 1.0f;

        positions[pointNdx] = Vec2(xf, yf);
    }
}

uint32_t LongRunningShaderCase::getSeed(const Params &params)
{
    const uint32_t seed = deStringHash(params.name) ^ deInt32Hash(params.shaderType) ^ deInt32Hash(params.loopType) ^
                          deInt32Hash(params.iterCountType) ^ deInt32Hash(params.minLoopIterCount) ^
                          deInt32Hash(params.maxLoopIterCount) ^ deInt32Hash(params.numInvocations);
    return seed;
}

LongRunningShaderCase::IterateResult LongRunningShaderCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    de::Random rnd(getSeed(*m_params));
    vector<Vec2> positions(m_params->numInvocations);
    vector<int> iterCounts(m_params->iterCountType == ITERCOUNTTYPE_DYNAMIC ? m_params->numInvocations : 1);
    vector<glu::VertexArrayBinding> vertexArrays;

    vertexArrays.push_back(glu::va::Float("a_position", 2, (int)positions.size(), 0, positions[0].getPtr()));
    if (m_params->iterCountType == ITERCOUNTTYPE_DYNAMIC)
        vertexArrays.push_back(glu::va::Int32("a_iterCount", 1, (int)iterCounts.size(), 0, &iterCounts[0]));

    genPositions(m_context.getRenderTarget(), (int)positions.size(), &positions[0]);

    for (vector<int>::iterator i = iterCounts.begin(); i != iterCounts.end(); ++i)
        *i = rnd.getInt(m_params->minLoopIterCount, m_params->maxLoopIterCount);

    gl.useProgram(m_program->getProgram());

    if (m_params->iterCountType == ITERCOUNTTYPE_UNIFORM)
        gl.uniform1i(gl.getUniformLocation(m_program->getProgram(), "u_iterCount"), iterCounts[0]);

    glu::draw(m_context.getRenderContext(), m_program->getProgram(), (int)vertexArrays.size(), &vertexArrays[0],
              glu::pr::Points(m_params->numInvocations));

    m_caseIterNdx += 1;
    return (m_caseIterNdx < m_numCaseIters) ? CONTINUE : STOP;
}

} // namespace

LongRunningShaderTests::LongRunningShaderTests(Context &context)
    : TestCaseGroup(context, "long_running_shaders", "Long-running shader stress tests")
{
}

LongRunningShaderTests::~LongRunningShaderTests(void)
{
}

void LongRunningShaderTests::init(void)
{
    const int numInvocations = 4096;
    const int shortLoopMin   = 5;
    const int shortLoopMax   = 10;
    const int mediumLoopMin  = 10000;
    const int mediumLoopMax  = 50000;
    const int longLoopMin    = 100000;
    const int longLoopMax    = 500000;

    static const LongRunningShaderCase::Params s_cases[] = {
        {"short_for_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_FOR, ITERCOUNTTYPE_DYNAMIC, numInvocations,
         shortLoopMin, shortLoopMax},
        {"short_for_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_FOR, ITERCOUNTTYPE_DYNAMIC, numInvocations,
         shortLoopMin, shortLoopMax},
        {"short_while_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_WHILE, ITERCOUNTTYPE_DYNAMIC, numInvocations,
         shortLoopMin, shortLoopMax},
        {"short_while_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_WHILE, ITERCOUNTTYPE_DYNAMIC, numInvocations,
         shortLoopMin, shortLoopMax},
        {"short_do_while_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_DO_WHILE, ITERCOUNTTYPE_DYNAMIC, numInvocations,
         shortLoopMin, shortLoopMax},
        {"short_do_while_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_DO_WHILE, ITERCOUNTTYPE_DYNAMIC,
         numInvocations, shortLoopMin, shortLoopMax},

        {"medium_static_for_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_FOR, ITERCOUNTTYPE_STATIC, numInvocations,
         mediumLoopMin, mediumLoopMax},
        {"medium_static_while_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_WHILE, ITERCOUNTTYPE_STATIC,
         numInvocations, mediumLoopMin, mediumLoopMax},
        {"medium_uniform_do_while_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_DO_WHILE, ITERCOUNTTYPE_UNIFORM,
         numInvocations, mediumLoopMin, mediumLoopMax},
        {"medium_uniform_for_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_FOR, ITERCOUNTTYPE_UNIFORM,
         numInvocations, mediumLoopMin, mediumLoopMax},

        {"medium_dynamic_for_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_FOR, ITERCOUNTTYPE_DYNAMIC, numInvocations,
         mediumLoopMin, mediumLoopMax},
        {"medium_dynamic_for_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_FOR, ITERCOUNTTYPE_DYNAMIC,
         numInvocations, mediumLoopMin, mediumLoopMax},
        {"medium_dynamic_while_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_WHILE, ITERCOUNTTYPE_DYNAMIC,
         numInvocations, mediumLoopMin, mediumLoopMax},
        {"medium_dynamic_while_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_WHILE, ITERCOUNTTYPE_DYNAMIC,
         numInvocations, mediumLoopMin, mediumLoopMax},
        {"medium_dynamic_do_while_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_DO_WHILE, ITERCOUNTTYPE_DYNAMIC,
         numInvocations, mediumLoopMin, mediumLoopMax},
        {"medium_dynamic_do_while_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_DO_WHILE, ITERCOUNTTYPE_DYNAMIC,
         numInvocations, mediumLoopMin, mediumLoopMax},

        {"long_static_while_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_WHILE, ITERCOUNTTYPE_STATIC, numInvocations,
         longLoopMin, longLoopMax},
        {"long_static_do_while_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_DO_WHILE, ITERCOUNTTYPE_STATIC,
         numInvocations, longLoopMin, longLoopMax},
        {"long_uniform_for_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_FOR, ITERCOUNTTYPE_UNIFORM, numInvocations,
         longLoopMin, longLoopMax},
        {"long_uniform_do_while_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_DO_WHILE, ITERCOUNTTYPE_UNIFORM,
         numInvocations, longLoopMin, longLoopMax},

        {"long_dynamic_for_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_FOR, ITERCOUNTTYPE_DYNAMIC, numInvocations,
         longLoopMin, longLoopMax},
        {"long_dynamic_for_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_FOR, ITERCOUNTTYPE_DYNAMIC, numInvocations,
         longLoopMin, longLoopMax},
        {"long_dynamic_while_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_WHILE, ITERCOUNTTYPE_DYNAMIC, numInvocations,
         longLoopMin, longLoopMax},
        {"long_dynamic_while_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_WHILE, ITERCOUNTTYPE_DYNAMIC,
         numInvocations, longLoopMin, longLoopMax},
        {"long_dynamic_do_while_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_DO_WHILE, ITERCOUNTTYPE_DYNAMIC,
         numInvocations, longLoopMin, longLoopMax},
        {"long_dynamic_do_while_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_DO_WHILE, ITERCOUNTTYPE_DYNAMIC,
         numInvocations, longLoopMin, longLoopMax},

        {"infinite_for_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_FOR, ITERCOUNTTYPE_DYNAMIC, numInvocations, -1,
         -1},
        {"infinite_for_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_FOR, ITERCOUNTTYPE_DYNAMIC, numInvocations, -1,
         -1},
        {"infinite_while_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_WHILE, ITERCOUNTTYPE_DYNAMIC, numInvocations, -1,
         -1},
        {"infinite_while_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_WHILE, ITERCOUNTTYPE_DYNAMIC, numInvocations,
         -1, -1},
        {"infinite_do_while_vertex", "", glu::SHADERTYPE_VERTEX, LOOPTYPE_DO_WHILE, ITERCOUNTTYPE_DYNAMIC,
         numInvocations, -1, -1},
        {"infinite_do_while_fragment", "", glu::SHADERTYPE_FRAGMENT, LOOPTYPE_DO_WHILE, ITERCOUNTTYPE_DYNAMIC,
         numInvocations, -1, -1},
    };

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_cases); ndx++)
        addChild(new LongRunningShaderCase(m_context, &s_cases[ndx]));
}

} // namespace Stress
} // namespace gles3
} // namespace deqp
