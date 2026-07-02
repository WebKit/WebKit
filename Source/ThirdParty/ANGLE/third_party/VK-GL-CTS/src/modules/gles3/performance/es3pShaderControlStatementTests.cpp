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
 * \brief Shader control statement performance tests.
 *//*--------------------------------------------------------------------*/

#include "es3pShaderControlStatementTests.hpp"
#include "glsShaderPerformanceCase.hpp"
#include "tcuTestLog.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include <string>
#include <vector>

namespace deqp
{
namespace gles3
{
namespace Performance
{

using namespace gls;
using namespace glw; // GL types
using std::string;
using std::vector;
using tcu::TestLog;
using tcu::Vec4;

// Writes the workload expression used in conditional tests.
static void writeConditionalWorkload(std::ostringstream &stream, const char *resultName, const char *operandName)
{
    const int numMultiplications = 64;

    stream << resultName << " = ";

    for (int i = 0; i < numMultiplications; i++)
    {
        if (i > 0)
            stream << "*";

        stream << operandName;
    }

    stream << ";";
}

// Writes the workload expression used in loop tests (one iteration).
static void writeLoopWorkload(std::ostringstream &stream, const char *resultName, const char *operandName)
{
    const int numMultiplications = 8;

    stream << resultName << " = ";

    for (int i = 0; i < numMultiplications; i++)
    {
        if (i > 0)
            stream << " * ";

        stream << "(" << resultName << " + " << operandName << ")";
    }

    stream << ";";
}

// The type of decision to be made in a conditional expression.
// \note In fragment cases with DECISION_ATTRIBUTE, the value in the expression will actually be a varying.
enum DecisionType
{
    DECISION_STATIC = 0,
    DECISION_UNIFORM,
    DECISION_ATTRIBUTE,

    DECISION_LAST
};

class ControlStatementCase : public ShaderPerformanceCase
{
public:
    ControlStatementCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                         const char *description, gls::PerfCaseType caseType)
        : ShaderPerformanceCase(testCtx, renderCtx, name, description, caseType)
    {
    }

    void init(void)
    {
        m_testCtx.getLog() << TestLog::Message << "Using additive blending." << TestLog::EndMessage;
        ShaderPerformanceCase::init();
    }

    void setupRenderState(void)
    {
        const glw::Functions &gl = m_renderCtx.getFunctions();

        gl.enable(GL_BLEND);
        gl.blendEquation(GL_FUNC_ADD);
        gl.blendFunc(GL_ONE, GL_ONE);
    }
};

class ConditionalCase : public ControlStatementCase
{
public:
    enum BranchResult
    {
        BRANCH_TRUE = 0,
        BRANCH_FALSE,
        BRANCH_MIXED,

        BRANCH_LAST
    };

    enum WorkloadDivision
    {
        WORKLOAD_DIVISION_EVEN = 0,    //! Both true and false branches contain same amount of computation.
        WORKLOAD_DIVISION_TRUE_HEAVY,  //! True branch contains more computation.
        WORKLOAD_DIVISION_FALSE_HEAVY, //! False branch contains more computation.

        WORKLOAD_DIVISION_LAST
    };

    ConditionalCase(Context &context, const char *name, const char *description, DecisionType decisionType,
                    BranchResult branchType, WorkloadDivision workloadDivision, bool isVertex);
    ~ConditionalCase(void);

    void init(void);
    void deinit(void);
    void setupProgram(uint32_t program);

private:
    DecisionType m_decisionType;
    BranchResult m_branchType;
    WorkloadDivision m_workloadDivision;

    vector<float>
        m_comparisonValueArray; // Will contain per-vertex comparison values if using mixed branch type in vertex case.
    uint32_t m_arrayBuffer;
};

ConditionalCase::ConditionalCase(Context &context, const char *name, const char *description, DecisionType decisionType,
                                 BranchResult branchType, WorkloadDivision workloadDivision, bool isVertex)
    : ControlStatementCase(context.getTestContext(), context.getRenderContext(), name, description,
                           isVertex ? CASETYPE_VERTEX : CASETYPE_FRAGMENT)
    , m_decisionType(decisionType)
    , m_branchType(branchType)
    , m_workloadDivision(workloadDivision)
    , m_arrayBuffer(0)
{
}

void ConditionalCase::init(void)
{
    bool isVertexCase = m_caseType == CASETYPE_VERTEX;

    bool isStaticCase    = m_decisionType == DECISION_STATIC;
    bool isUniformCase   = m_decisionType == DECISION_UNIFORM;
    bool isAttributeCase = m_decisionType == DECISION_ATTRIBUTE;

    DE_ASSERT(isStaticCase || isUniformCase || isAttributeCase);

    bool isConditionTrue  = m_branchType == BRANCH_TRUE;
    bool isConditionFalse = m_branchType == BRANCH_FALSE;
    bool isConditionMixed = m_branchType == BRANCH_MIXED;

    DE_ASSERT(isConditionTrue || isConditionFalse || isConditionMixed);
    DE_UNREF(isConditionFalse);

    DE_ASSERT(isAttributeCase ||
              !isConditionMixed); // The branch taken can vary between executions only if using attribute input.

    const char *staticCompareValueStr = isConditionTrue ? "1.0" : "-1.0";
    const char *compareValueStr       = isStaticCase  ? staticCompareValueStr :
                                        isUniformCase ? "u_compareValue" :
                                        isVertexCase  ? "a_compareValue" :
                                                        "v_compareValue";

    std::ostringstream vtx;
    std::ostringstream frag;
    std::ostringstream &op = isVertexCase ? vtx : frag;

    vtx << "#version 300 es\n";
    vtx << "in highp vec4 a_position;\n"; // Position attribute.
    vtx << "in mediump vec4 a_value0;\n"; // Input for workload calculations of "true" branch.
    vtx << "in mediump vec4 a_value1;\n"; // Input for workload calculations of "false" branch.

    frag << "#version 300 es\n";
    frag << "layout(location = 0) out mediump vec4 o_color;\n";

    // Value to be used in the conditional expression.
    if (isAttributeCase)
        vtx << "in mediump float a_compareValue;\n";
    else if (isUniformCase)
        op << "uniform mediump float u_compareValue;\n";

    // Varyings.
    if (isVertexCase)
    {
        vtx << "out mediump vec4 v_color;\n";
        frag << "in mediump vec4 v_color;\n";
    }
    else
    {
        vtx << "out mediump vec4 v_value0;\n";
        vtx << "out mediump vec4 v_value1;\n";
        frag << "in mediump vec4 v_value0;\n";
        frag << "in mediump vec4 v_value1;\n";

        if (isAttributeCase)
        {
            vtx << "out mediump float v_compareValue;\n";
            frag << "in mediump float v_compareValue;\n";
        }
    }

    vtx << "\n";
    vtx << "void main()\n";
    vtx << "{\n";
    vtx << "    gl_Position = a_position;\n";

    frag << "\n";
    frag << "void main()\n";
    frag << "{\n";

    op << "    mediump vec4 res;\n";

    string condition;

    if (isConditionMixed && !isVertexCase)
        condition =
            string("") + "fract(" + compareValueStr + ") < 0.5"; // Comparison result varies with high frequency.
    else
        condition = string("") + compareValueStr + " > 0.0";

    op << "    if (" << condition << ")\n";
    op << "    {\n";

    op << "\t\t";
    if (m_workloadDivision == WORKLOAD_DIVISION_EVEN || m_workloadDivision == WORKLOAD_DIVISION_TRUE_HEAVY)
        writeConditionalWorkload(op, "res",
                                 isVertexCase ? "a_value0" : "v_value0"); // Workload calculation for the "true" branch.
    else
        op << "res = " << (isVertexCase ? "a_value0" : "v_value0") << ";";
    op << "\n";

    op << "    }\n";
    op << "    else\n";
    op << "    {\n";

    op << "\t\t";
    if (m_workloadDivision == WORKLOAD_DIVISION_EVEN || m_workloadDivision == WORKLOAD_DIVISION_FALSE_HEAVY)
        writeConditionalWorkload(
            op, "res", isVertexCase ? "a_value1" : "v_value1"); // Workload calculations for the "false" branch.
    else
        op << "res = " << (isVertexCase ? "a_value1" : "v_value1") << ";";
    op << "\n";

    op << "    }\n";

    if (isVertexCase)
    {
        // Put result to color variable.
        vtx << "    v_color = res;\n";
        frag << "    o_color = v_color;\n";
    }
    else
    {
        // Transfer inputs to fragment shader through varyings.
        if (isAttributeCase)
            vtx << "    v_compareValue = a_compareValue;\n";
        vtx << "    v_value0 = a_value0;\n";
        vtx << "    v_value1 = a_value1;\n";

        frag << "    o_color = res;\n"; // Put result to color variable.
    }

    vtx << "}\n";
    frag << "}\n";

    m_vertShaderSource = vtx.str();
    m_fragShaderSource = frag.str();

    if (isAttributeCase)
    {
        if (!isConditionMixed)
        {
            // Every execution takes the same branch.

            float value = isConditionTrue ? +1.0f : -1.0f;
            m_attributes.push_back(AttribSpec("a_compareValue", Vec4(value, 0.0f, 0.0f, 0.0f),
                                              Vec4(value, 0.0f, 0.0f, 0.0f), Vec4(value, 0.0f, 0.0f, 0.0f),
                                              Vec4(value, 0.0f, 0.0f, 0.0f)));
        }
        else if (isVertexCase)
        {
            // Vertex case, not every execution takes the same branch.

            const int numComponents = 4;
            int numVertices         = (getGridWidth() + 1) * (getGridHeight() + 1);

            // setupProgram() will later bind this array as an attribute.
            m_comparisonValueArray.resize(numVertices * numComponents);

            // Make every second vertex take the true branch, and every second the false branch.
            for (int i = 0; i < (int)m_comparisonValueArray.size(); i++)
            {
                if (i % numComponents == 0)
                    m_comparisonValueArray[i] = (i / numComponents) % 2 == 0 ? +1.0f : -1.0f;
                else
                    m_comparisonValueArray[i] = 0.0f;
            }
        }
        else // isConditionMixed && !isVertexCase
        {
            // Fragment case, not every execution takes the same branch.
            // \note fract(a_compareValue) < 0.5 will be true for every second column of fragments.

            float minValue = 0.0f;
            float maxValue = (float)getViewportWidth() * 0.5f;
            m_attributes.push_back(AttribSpec("a_compareValue", Vec4(minValue, 0.0f, 0.0f, 0.0f),
                                              Vec4(maxValue, 0.0f, 0.0f, 0.0f), Vec4(minValue, 0.0f, 0.0f, 0.0f),
                                              Vec4(maxValue, 0.0f, 0.0f, 0.0f)));
        }
    }

    m_attributes.push_back(AttribSpec("a_value0", Vec4(0.0f, 0.1f, 0.2f, 0.3f), Vec4(0.4f, 0.5f, 0.6f, 0.7f),
                                      Vec4(0.8f, 0.9f, 1.0f, 1.1f), Vec4(1.2f, 1.3f, 1.4f, 1.5f)));

    m_attributes.push_back(AttribSpec("a_value1", Vec4(0.0f, 0.1f, 0.2f, 0.3f), Vec4(0.4f, 0.5f, 0.6f, 0.7f),
                                      Vec4(0.8f, 0.9f, 1.0f, 1.1f), Vec4(1.2f, 1.3f, 1.4f, 1.5f)));

    ControlStatementCase::init();
}

void ConditionalCase::setupProgram(uint32_t program)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    if (m_decisionType == DECISION_UNIFORM)
    {
        int location = gl.getUniformLocation(program, "u_compareValue");
        gl.uniform1f(location, m_branchType == BRANCH_TRUE ? +1.0f : -1.0f);
    }
    else if (m_decisionType == DECISION_ATTRIBUTE && m_branchType == BRANCH_MIXED && m_caseType == CASETYPE_VERTEX)
    {
        // Setup per-vertex comparison values calculated in init().

        const int numComponents   = 4;
        int compareAttribLocation = gl.getAttribLocation(program, "a_compareValue");

        DE_ASSERT((int)m_comparisonValueArray.size() == numComponents * (getGridWidth() + 1) * (getGridHeight() + 1));

        gl.genBuffers(1, &m_arrayBuffer);
        gl.bindBuffer(GL_ARRAY_BUFFER, m_arrayBuffer);
        gl.bufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(m_comparisonValueArray.size() * sizeof(float)),
                      &m_comparisonValueArray[0], GL_STATIC_DRAW);
        gl.enableVertexAttribArray(compareAttribLocation);
        gl.vertexAttribPointer(compareAttribLocation, (GLint)numComponents, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Setup program state");
}

ConditionalCase::~ConditionalCase(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    if (m_arrayBuffer != 0)
    {
        gl.deleteBuffers(1, &m_arrayBuffer);
        m_arrayBuffer = 0;
    }
}

void ConditionalCase::deinit(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    m_comparisonValueArray.clear();

    if (m_arrayBuffer != 0)
    {
        gl.deleteBuffers(1, &m_arrayBuffer);
        m_arrayBuffer = 0;
    }

    ShaderPerformanceCase::deinit();
}

class LoopCase : public ControlStatementCase
{
public:
    enum LoopType
    {
        LOOP_FOR = 0,
        LOOP_WHILE,
        LOOP_DO_WHILE,

        LOOP_LAST
    };
    LoopCase(Context &context, const char *name, const char *description, LoopType type, DecisionType decisionType,
             bool isLoopBoundStable, bool isVertex);
    ~LoopCase(void);

    void init(void);
    void deinit(void);
    void setupProgram(uint32_t program);

private:
    DecisionType m_decisionType;
    LoopType m_type;

    bool m_isLoopBoundStable;   // Whether loop bound is same in all executions.
    vector<float> m_boundArray; // Will contain per-vertex loop bounds if using non-stable attribute in vertex case.
    uint32_t m_arrayBuffer;
};

LoopCase::LoopCase(Context &context, const char *name, const char *description, LoopType type,
                   DecisionType decisionType, bool isLoopBoundStable, bool isVertex)
    : ControlStatementCase(context.getTestContext(), context.getRenderContext(), name, description,
                           isVertex ? CASETYPE_VERTEX : CASETYPE_FRAGMENT)
    , m_decisionType(decisionType)
    , m_type(type)
    , m_isLoopBoundStable(isLoopBoundStable)
    , m_arrayBuffer(0)
{
}

void LoopCase::init(void)
{
    bool isVertexCase = m_caseType == CASETYPE_VERTEX;

    bool isStaticCase    = m_decisionType == DECISION_STATIC;
    bool isUniformCase   = m_decisionType == DECISION_UNIFORM;
    bool isAttributeCase = m_decisionType == DECISION_ATTRIBUTE;

    DE_ASSERT(isStaticCase || isUniformCase || isAttributeCase);

    DE_ASSERT(m_type == LOOP_FOR || m_type == LOOP_WHILE || m_type == LOOP_DO_WHILE);

    DE_ASSERT(isAttributeCase ||
              m_isLoopBoundStable); // The loop bound count can vary between executions only if using attribute input.

    // \note The fractional part is .5 (instead of .0) so that these can be safely used as loop bounds.
    const float loopBound                   = 10.5f;
    const float unstableBoundLow            = 5.5f;
    const float unstableBoundHigh           = 15.5f;
    static const char *loopBoundStr         = "10.5";
    static const char *unstableBoundLowStr  = "5.5";
    static const char *unstableBoundHighStr = "15.5";

    const char *boundValueStr = isStaticCase        ? loopBoundStr :
                                isUniformCase       ? "u_bound" :
                                isVertexCase        ? "a_bound" :
                                m_isLoopBoundStable ? "v_bound" :
                                                      "loopBound";

    std::ostringstream vtx;
    std::ostringstream frag;
    std::ostringstream &op = isVertexCase ? vtx : frag;

    vtx << "#version 300 es\n";
    vtx << "in highp vec4 a_position;\n"; // Position attribute.
    vtx << "in mediump vec4 a_value;\n";  // Input for workload calculations.

    frag << "#version 300 es\n";
    frag << "layout(location = 0) out mediump vec4 o_color;\n";

    // Value to be used as the loop iteration count.
    if (isAttributeCase)
        vtx << "in mediump float a_bound;\n";
    else if (isUniformCase)
        op << "uniform mediump float u_bound;\n";

    // Varyings.
    if (isVertexCase)
    {
        vtx << "out mediump vec4 v_color;\n";
        frag << "in mediump vec4 v_color;\n";
    }
    else
    {
        vtx << "out mediump vec4 v_value;\n";
        frag << "in mediump vec4 v_value;\n";

        if (isAttributeCase)
        {
            vtx << "out mediump float v_bound;\n";
            frag << "in mediump float v_bound;\n";
        }
    }

    vtx << "\n";
    vtx << "void main()\n";
    vtx << "{\n";
    vtx << "    gl_Position = a_position;\n";

    frag << "\n";
    frag << "void main()\n";
    frag << "{\n";

    op << "    mediump vec4 res = vec4(0.0);\n";

    if (!m_isLoopBoundStable && !isVertexCase)
    {
        // Choose the actual loop bound based on v_bound.
        // \note Loop bound will vary with high frequency between fragment columns, given appropriate range for v_bound.
        op << "    mediump float loopBound = fract(v_bound) < 0.5 ? " << unstableBoundLowStr << " : "
           << unstableBoundHighStr << ";\n";
    }

    // Start a for, while or do-while loop.
    if (m_type == LOOP_FOR)
        op << "    for (mediump float i = 0.0; i < " << boundValueStr << "; i++)\n";
    else
    {
        op << "    mediump float i = 0.0;\n";
        if (m_type == LOOP_WHILE)
            op << "    while (i < " << boundValueStr << ")\n";
        else // LOOP_DO_WHILE
            op << "    do\n";
    }
    op << "    {\n";

    // Workload calculations inside the loop.
    op << "\t\t";
    writeLoopWorkload(op, "res", isVertexCase ? "a_value" : "v_value");
    op << "\n";

    // Only "for" has counter increment in the loop head.
    if (m_type != LOOP_FOR)
        op << "        i++;\n";

    // End the loop.
    if (m_type == LOOP_DO_WHILE)
        op << "    } while (i < " << boundValueStr << ");\n";
    else
        op << "    }\n";

    if (isVertexCase)
    {
        // Put result to color variable.
        vtx << "    v_color = res;\n";
        frag << "    o_color = v_color;\n";
    }
    else
    {
        // Transfer inputs to fragment shader through varyings.
        if (isAttributeCase)
            vtx << "    v_bound = a_bound;\n";
        vtx << "    v_value = a_value;\n";

        frag << "    o_color = res;\n"; // Put result to color variable.
    }

    vtx << "}\n";
    frag << "}\n";

    m_vertShaderSource = vtx.str();
    m_fragShaderSource = frag.str();

    if (isAttributeCase)
    {
        if (m_isLoopBoundStable)
        {
            // Every execution has same number of iterations.

            m_attributes.push_back(AttribSpec("a_bound", Vec4(loopBound, 0.0f, 0.0f, 0.0f),
                                              Vec4(loopBound, 0.0f, 0.0f, 0.0f), Vec4(loopBound, 0.0f, 0.0f, 0.0f),
                                              Vec4(loopBound, 0.0f, 0.0f, 0.0f)));
        }
        else if (isVertexCase)
        {
            // Vertex case, with non-constant number of iterations.

            const int numComponents = 4;
            int numVertices         = (getGridWidth() + 1) * (getGridHeight() + 1);

            // setupProgram() will later bind this array as an attribute.
            m_boundArray.resize(numVertices * numComponents);

            // Vary between low and high loop bounds; they should average to loopBound however.
            for (int i = 0; i < (int)m_boundArray.size(); i++)
            {
                if (i % numComponents == 0)
                    m_boundArray[i] = (i / numComponents) % 2 == 0 ? unstableBoundLow : unstableBoundHigh;
                else
                    m_boundArray[i] = 0.0f;
            }
        }
        else // !m_isLoopBoundStable && !isVertexCase
        {
            // Fragment case, with non-constant number of iterations.
            // \note fract(a_bound) < 0.5 will be true for every second fragment.

            float minValue = 0.0f;
            float maxValue = (float)getViewportWidth() * 0.5f;
            m_attributes.push_back(AttribSpec("a_bound", Vec4(minValue, 0.0f, 0.0f, 0.0f),
                                              Vec4(maxValue, 0.0f, 0.0f, 0.0f), Vec4(minValue, 0.0f, 0.0f, 0.0f),
                                              Vec4(maxValue, 0.0f, 0.0f, 0.0f)));
        }
    }

    m_attributes.push_back(AttribSpec("a_value", Vec4(0.0f, 0.1f, 0.2f, 0.3f), Vec4(0.4f, 0.5f, 0.6f, 0.7f),
                                      Vec4(0.8f, 0.9f, 1.0f, 1.1f), Vec4(1.2f, 1.3f, 1.4f, 1.5f)));

    ControlStatementCase::init();
}

void LoopCase::setupProgram(uint32_t program)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    if (m_decisionType == DECISION_UNIFORM)
    {
        const float loopBound = 10.5f;

        int location = gl.getUniformLocation(program, "u_bound");
        gl.uniform1f(location, loopBound);
    }
    else if (m_decisionType == DECISION_ATTRIBUTE && !m_isLoopBoundStable && m_caseType == CASETYPE_VERTEX)
    {
        // Setup per-vertex loop bounds calculated in init().

        const int numComponents = 4;
        int boundAttribLocation = gl.getAttribLocation(program, "a_bound");

        DE_ASSERT((int)m_boundArray.size() == numComponents * (getGridWidth() + 1) * (getGridHeight() + 1));

        gl.genBuffers(1, &m_arrayBuffer);
        gl.bindBuffer(GL_ARRAY_BUFFER, m_arrayBuffer);
        gl.bufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(m_boundArray.size() * sizeof(float)), &m_boundArray[0],
                      GL_STATIC_DRAW);
        gl.enableVertexAttribArray(boundAttribLocation);
        gl.vertexAttribPointer(boundAttribLocation, (GLint)numComponents, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Setup program state");
}

LoopCase::~LoopCase(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    if (m_arrayBuffer)
    {
        gl.deleteBuffers(1, &m_arrayBuffer);
        m_arrayBuffer = 0;
    }
}

void LoopCase::deinit(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    m_boundArray.clear();

    if (m_arrayBuffer)
    {
        gl.deleteBuffers(1, &m_arrayBuffer);
        m_arrayBuffer = 0;
    }

    ShaderPerformanceCase::deinit();
}

// A reference case, same calculations as the actual tests but without control statements.
class WorkloadReferenceCase : public ControlStatementCase
{
public:
    WorkloadReferenceCase(Context &context, const char *name, const char *description, bool isVertex);

    void init(void);

protected:
    virtual void writeWorkload(std::ostringstream &dst, const char *resultVariableName,
                               const char *inputVariableName) const = 0;
};

WorkloadReferenceCase::WorkloadReferenceCase(Context &context, const char *name, const char *description, bool isVertex)
    : ControlStatementCase(context.getTestContext(), context.getRenderContext(), name, description,
                           isVertex ? CASETYPE_VERTEX : CASETYPE_FRAGMENT)
{
}

void WorkloadReferenceCase::init(void)
{
    bool isVertexCase = m_caseType == CASETYPE_VERTEX;

    std::ostringstream vtx;
    std::ostringstream frag;
    std::ostringstream &op = isVertexCase ? vtx : frag;

    vtx << "#version 300 es\n";
    vtx << "in highp vec4 a_position;\n"; // Position attribute.
    vtx << "in mediump vec4 a_value;\n";  // Value for workload calculations.

    frag << "#version 300 es\n";
    frag << "layout(location = 0) out mediump vec4 o_color;\n";

    // Varyings.
    if (isVertexCase)
    {
        vtx << "out mediump vec4 v_color;\n";
        frag << "in mediump vec4 v_color;\n";
    }
    else
    {
        vtx << "out mediump vec4 v_value;\n";
        frag << "in mediump vec4 v_value;\n";
    }

    vtx << "\n";
    vtx << "void main()\n";
    vtx << "{\n";
    vtx << "    gl_Position = a_position;\n";

    frag << "\n";
    frag << "void main()\n";
    frag << "{\n";

    op << "\tmediump vec4 res;\n";
    writeWorkload(op, "res", isVertexCase ? "a_value" : "v_value");

    if (isVertexCase)
    {
        // Put result to color variable.
        vtx << "    v_color = res;\n";
        frag << "    o_color = v_color;\n";
    }
    else
    {
        vtx << "    v_value = a_value;\n"; // Transfer input to fragment shader through varying.
        frag << "    o_color = res;\n";    // Put result to color variable.
    }

    vtx << "}\n";
    frag << "}\n";

    m_vertShaderSource = vtx.str();
    m_fragShaderSource = frag.str();

    m_attributes.push_back(AttribSpec("a_value", Vec4(0.0f, 0.1f, 0.2f, 0.3f), Vec4(0.4f, 0.5f, 0.6f, 0.7f),
                                      Vec4(0.8f, 0.9f, 1.0f, 1.1f), Vec4(1.2f, 1.3f, 1.4f, 1.5f)));

    ControlStatementCase::init();
}

class LoopWorkloadReferenceCase : public WorkloadReferenceCase
{
public:
    LoopWorkloadReferenceCase(Context &context, const char *name, const char *description, bool isAttributeStable,
                              bool isVertex)
        : WorkloadReferenceCase(context, name, description, isVertex)
        , m_isAttributeStable(isAttributeStable)
    {
    }

protected:
    void writeWorkload(std::ostringstream &dst, const char *resultVariableName, const char *inputVariableName) const;

private:
    bool m_isAttributeStable;
};

void LoopWorkloadReferenceCase::writeWorkload(std::ostringstream &dst, const char *resultVariableName,
                                              const char *inputVariableName) const
{
    const int loopIterations = 11;
    bool isVertexCase        = m_caseType == CASETYPE_VERTEX;

    dst << "\t" << resultVariableName << " = vec4(0.0);\n";

    for (int i = 0; i < loopIterations; i++)
    {
        dst << "\t";
        writeLoopWorkload(dst, resultVariableName, inputVariableName);
        dst << "\n";
    }

    if (!isVertexCase && !m_isAttributeStable)
    {
        // Corresponds to the fract() done in a real test's fragment case with non-stable attribute.
        dst << "    res.x = fract(res.x);\n";
    }
}

class ConditionalWorkloadReferenceCase : public WorkloadReferenceCase
{
public:
    ConditionalWorkloadReferenceCase(Context &context, const char *name, const char *description,
                                     bool isAttributeStable, bool isVertex)
        : WorkloadReferenceCase(context, name, description, isVertex)
        , m_isAttributeStable(isAttributeStable)
    {
    }

protected:
    void writeWorkload(std::ostringstream &dst, const char *resultVariableName, const char *inputVariableName) const;

private:
    bool m_isAttributeStable;
};

void ConditionalWorkloadReferenceCase::writeWorkload(std::ostringstream &dst, const char *resultVariableName,
                                                     const char *inputVariableName) const
{
    bool isVertexCase = m_caseType == CASETYPE_VERTEX;

    dst << "\t";
    writeConditionalWorkload(dst, resultVariableName, inputVariableName);
    dst << "\n";

    if (!isVertexCase && !m_isAttributeStable)
    {
        // Corresponds to the fract() done in a real test's fragment case with non-stable attribute.
        dst << "    res.x = fract(res.x);\n";
    }
}

// A workload reference case for e.g. a conditional case with a branch with no computation.
class EmptyWorkloadReferenceCase : public WorkloadReferenceCase
{
public:
    EmptyWorkloadReferenceCase(Context &context, const char *name, const char *description, bool isVertex)
        : WorkloadReferenceCase(context, name, description, isVertex)
    {
    }

protected:
    void writeWorkload(std::ostringstream &dst, const char *resultVariableName, const char *inputVariableName) const
    {
        dst << "\t" << resultVariableName << " = " << inputVariableName << ";\n";
    }
};

ShaderControlStatementTests::ShaderControlStatementTests(Context &context)
    : TestCaseGroup(context, "control_statement", "Control Statement Performance Tests")
{
}

ShaderControlStatementTests::~ShaderControlStatementTests(void)
{
}

void ShaderControlStatementTests::init(void)
{
    // Conditional cases (if-else).

    tcu::TestCaseGroup *ifElseGroup =
        new tcu::TestCaseGroup(m_testCtx, "if_else", "if-else Conditional Performance Tests");
    addChild(ifElseGroup);

    for (int isFrag = 0; isFrag <= 1; isFrag++)
    {
        bool isVertex = isFrag == 0;
        ShaderPerformanceCaseGroup *vertexOrFragmentGroup =
            new ShaderPerformanceCaseGroup(m_testCtx, isVertex ? "vertex" : "fragment", "");
        ifElseGroup->addChild(vertexOrFragmentGroup);

        DE_STATIC_ASSERT(DECISION_STATIC == 0);
        for (int decisionType = (int)DECISION_STATIC; decisionType < (int)DECISION_LAST; decisionType++)
        {
            const char *decisionName = decisionType == (int)DECISION_STATIC    ? "static" :
                                       decisionType == (int)DECISION_UNIFORM   ? "uniform" :
                                       decisionType == (int)DECISION_ATTRIBUTE ? (isVertex ? "attribute" : "varying") :
                                                                                 nullptr;
            DE_ASSERT(decisionName != nullptr);

            for (int workloadDivision = 0; workloadDivision < ConditionalCase::WORKLOAD_DIVISION_LAST;
                 workloadDivision++)
            {
                const char *workloadDivisionSuffix =
                    workloadDivision == (int)ConditionalCase::WORKLOAD_DIVISION_EVEN        ? "" :
                    workloadDivision == (int)ConditionalCase::WORKLOAD_DIVISION_TRUE_HEAVY  ? "_with_heavier_true" :
                    workloadDivision == (int)ConditionalCase::WORKLOAD_DIVISION_FALSE_HEAVY ? "_with_heavier_false" :
                                                                                              nullptr;
                DE_ASSERT(workloadDivisionSuffix != nullptr);

                DE_STATIC_ASSERT(ConditionalCase::BRANCH_TRUE == 0);
                for (int branchResult = (int)ConditionalCase::BRANCH_TRUE;
                     branchResult < (int)ConditionalCase::BRANCH_LAST; branchResult++)
                {
                    if (decisionType != (int)DECISION_ATTRIBUTE && branchResult == (int)ConditionalCase::BRANCH_MIXED)
                        continue;

                    const char *branchResultName = branchResult == (int)ConditionalCase::BRANCH_TRUE  ? "true" :
                                                   branchResult == (int)ConditionalCase::BRANCH_FALSE ? "false" :
                                                   branchResult == (int)ConditionalCase::BRANCH_MIXED ? "mixed" :
                                                                                                        nullptr;
                    DE_ASSERT(branchResultName != nullptr);

                    string caseName = string("") + decisionName + "_" + branchResultName + workloadDivisionSuffix;

                    vertexOrFragmentGroup->addChild(
                        new ConditionalCase(m_context, caseName.c_str(), "", (DecisionType)decisionType,
                                            (ConditionalCase::BranchResult)branchResult,
                                            (ConditionalCase::WorkloadDivision)workloadDivision, isVertex));
                }
            }
        }

        if (isVertex)
            vertexOrFragmentGroup->addChild(
                new ConditionalWorkloadReferenceCase(m_context, "reference", "", true, isVertex));
        else
        {
            // Only fragment case with BRANCH_MIXED has an additional fract() call.
            vertexOrFragmentGroup->addChild(
                new ConditionalWorkloadReferenceCase(m_context, "reference_unmixed", "", true, isVertex));
            vertexOrFragmentGroup->addChild(
                new ConditionalWorkloadReferenceCase(m_context, "reference_mixed", "", false, isVertex));
        }

        vertexOrFragmentGroup->addChild(new EmptyWorkloadReferenceCase(m_context, "reference_empty", "", isVertex));
    }

    // Loop cases.

    static const struct
    {
        LoopCase::LoopType type;
        const char *name;
        const char *description;
    } loopGroups[] = {{LoopCase::LOOP_FOR, "for", "for Loop Performance Tests"},
                      {LoopCase::LOOP_WHILE, "while", "while Loop Performance Tests"},
                      {LoopCase::LOOP_DO_WHILE, "do_while", "do-while Loop Performance Tests"}};

    for (int groupNdx = 0; groupNdx < DE_LENGTH_OF_ARRAY(loopGroups); groupNdx++)
    {
        tcu::TestCaseGroup *currentLoopGroup =
            new tcu::TestCaseGroup(m_testCtx, loopGroups[groupNdx].name, loopGroups[groupNdx].description);
        addChild(currentLoopGroup);

        for (int isFrag = 0; isFrag <= 1; isFrag++)
        {
            bool isVertex = isFrag == 0;
            ShaderPerformanceCaseGroup *vertexOrFragmentGroup =
                new ShaderPerformanceCaseGroup(m_testCtx, isVertex ? "vertex" : "fragment", "");
            currentLoopGroup->addChild(vertexOrFragmentGroup);

            DE_STATIC_ASSERT(DECISION_STATIC == 0);
            for (int decisionType = (int)DECISION_STATIC; decisionType < (int)DECISION_LAST; decisionType++)
            {
                const char *decisionName =
                    decisionType == (int)DECISION_STATIC    ? "static" :
                    decisionType == (int)DECISION_UNIFORM   ? "uniform" :
                    decisionType == (int)DECISION_ATTRIBUTE ? (isVertex ? "attribute" : "varying") :
                                                              nullptr;
                DE_ASSERT(decisionName != nullptr);

                if (decisionType == (int)DECISION_ATTRIBUTE)
                {
                    vertexOrFragmentGroup->addChild(new LoopCase(m_context, (string(decisionName) + "_stable").c_str(),
                                                                 "", loopGroups[groupNdx].type,
                                                                 (DecisionType)decisionType, true, isVertex));
                    vertexOrFragmentGroup->addChild(
                        new LoopCase(m_context, (string(decisionName) + "_unstable").c_str(), "",
                                     loopGroups[groupNdx].type, (DecisionType)decisionType, false, isVertex));
                }
                else
                    vertexOrFragmentGroup->addChild(new LoopCase(m_context, decisionName, "", loopGroups[groupNdx].type,
                                                                 (DecisionType)decisionType, true, isVertex));
            }

            if (isVertex)
                vertexOrFragmentGroup->addChild(
                    new LoopWorkloadReferenceCase(m_context, "reference", "", true, isVertex));
            else
            {
                // Only fragment case with unstable attribute has an additional fract() call.
                vertexOrFragmentGroup->addChild(
                    new LoopWorkloadReferenceCase(m_context, "reference_stable", "", true, isVertex));
                vertexOrFragmentGroup->addChild(
                    new LoopWorkloadReferenceCase(m_context, "reference_unstable", "", false, isVertex));
            }
        }
    }
}

} // namespace Performance
} // namespace gles3
} // namespace deqp
