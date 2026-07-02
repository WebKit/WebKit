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
 * \brief Optimized vs unoptimized shader performance tests.
 *//*--------------------------------------------------------------------*/

#include "es2pShaderOptimizationTests.hpp"
#include "glsShaderPerformanceMeasurer.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuStringTemplate.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"
#include "deMath.h"

#include "glwFunctions.hpp"

#include <vector>
#include <string>
#include <map>

using de::SharedPtr;
using de::toString;
using glu::ShaderProgram;
using tcu::TestLog;
using tcu::Vec4;

using std::string;
using std::vector;

namespace deqp
{

using gls::ShaderPerformanceMeasurer;

namespace gles2
{
namespace Performance
{

static inline std::map<string, string> singleMap(const string &key, const string &value)
{
    std::map<string, string> res;
    res[key] = value;
    return res;
}

static inline string repeat(const string &str, int numRepeats, const string &delim = "")
{
    string result = str;
    for (int i = 1; i < numRepeats; i++)
        result += delim + str;
    return result;
}

static inline string repeatIndexedTemplate(const string &strTempl, int numRepeats, const string &delim = "",
                                           int ndxStart = 0)
{
    const tcu::StringTemplate templ(strTempl);
    string result;
    std::map<string, string> params;

    for (int i = 0; i < numRepeats; i++)
    {
        params["PREV_NDX"] = toString(i + ndxStart - 1);
        params["NDX"]      = toString(i + ndxStart);

        result += (i > 0 ? delim : "") + templ.specialize(params);
    }

    return result;
}

namespace
{

enum CaseShaderType
{
    CASESHADERTYPE_VERTEX = 0,
    CASESHADERTYPE_FRAGMENT,

    CASESHADERTYPE_LAST
};

static inline string getShaderPrecision(CaseShaderType shaderType)
{
    switch (shaderType)
    {
    case CASESHADERTYPE_VERTEX:
        return "highp";
    case CASESHADERTYPE_FRAGMENT:
        return "mediump";
    default:
        DE_ASSERT(false);
        return "";
    }
}

struct ProgramData
{
    glu::ProgramSources sources;
    vector<gls::AttribSpec>
        attributes; //!< \note Shouldn't contain a_position; that one is set by gls::ShaderPerformanceMeasurer.

    ProgramData(void)
    {
    }
    ProgramData(const glu::ProgramSources &sources_,
                const vector<gls::AttribSpec> &attributes_ = vector<gls::AttribSpec>())
        : sources(sources_)
        , attributes(attributes_)
    {
    }
    ProgramData(const glu::ProgramSources &sources_, const gls::AttribSpec &attribute)
        : sources(sources_)
        , attributes(1, attribute)
    {
    }
};

//! Shader boilerplate helper; most cases have similar basic shader structure.
static inline ProgramData defaultProgramData(CaseShaderType shaderType, const string &funcDefs,
                                             const string &mainStatements)
{
    const bool isVertexCase   = shaderType == CASESHADERTYPE_VERTEX;
    const bool isFragmentCase = shaderType == CASESHADERTYPE_FRAGMENT;
    const string vtxPrec      = getShaderPrecision(CASESHADERTYPE_VERTEX);
    const string fragPrec     = getShaderPrecision(CASESHADERTYPE_FRAGMENT);

    return ProgramData(glu::ProgramSources()
                           << glu::VertexSource("attribute " + vtxPrec +
                                                " vec4 a_position;\n"
                                                "attribute " +
                                                vtxPrec +
                                                " vec4 a_value;\n"
                                                "varying " +
                                                fragPrec + " vec4 v_value;\n" + (isVertexCase ? funcDefs : "") +
                                                "void main (void)\n"
                                                "{\n"
                                                "    gl_Position = a_position;\n"
                                                "    " +
                                                vtxPrec + " vec4 value = a_value;\n" +
                                                (isVertexCase ? mainStatements : "") +
                                                "    v_value = value;\n"
                                                "}\n")

                           << glu::FragmentSource(
                                  "varying " + fragPrec + " vec4 v_value;\n" + (isFragmentCase ? funcDefs : "") +
                                  "void main (void)\n"
                                  "{\n"
                                  "    " +
                                  fragPrec + " vec4 value = v_value;\n" + (isFragmentCase ? mainStatements : "") +
                                  "    gl_FragColor = value;\n"
                                  "}\n"),
                       gls::AttribSpec("a_value", Vec4(1.0f, 0.0f, 0.0f, 0.0f), Vec4(0.0f, 1.0f, 0.0f, 0.0f),
                                       Vec4(0.0f, 0.0f, 1.0f, 0.0f), Vec4(0.0f, 0.0f, 0.0f, 1.0f)));
}

static inline ProgramData defaultProgramData(CaseShaderType shaderType, const string &mainStatements)
{
    return defaultProgramData(shaderType, "", mainStatements);
}

class ShaderOptimizationCase : public TestCase
{
public:
    ShaderOptimizationCase(Context &context, const char *name, const char *description, CaseShaderType caseShaderType)
        : TestCase(context, tcu::NODETYPE_PERFORMANCE, name, description)
        , m_caseShaderType(caseShaderType)
        , m_state(STATE_LAST)
        , m_measurer(context.getRenderContext(), caseShaderType == CASESHADERTYPE_VERTEX   ? gls::CASETYPE_VERTEX :
                                                 caseShaderType == CASESHADERTYPE_FRAGMENT ? gls::CASETYPE_FRAGMENT :
                                                                                             gls::CASETYPE_LAST)
        , m_unoptimizedResult(-1.0f, -1.0f)
        , m_optimizedResult(-1.0f, -1.0f)
    {
    }

    virtual ~ShaderOptimizationCase(void)
    {
    }

    void init(void);
    IterateResult iterate(void);

protected:
    virtual ProgramData generateProgramData(bool optimized) const = 0;

    const CaseShaderType m_caseShaderType;

private:
    enum State
    {
        STATE_INIT_UNOPTIMIZED = 0,
        STATE_MEASURE_UNOPTIMIZED,
        STATE_INIT_OPTIMIZED,
        STATE_MEASURE_OPTIMIZED,
        STATE_FINISHED,

        STATE_LAST
    };

    ProgramData &programData(bool optimized)
    {
        return optimized ? m_optimizedData : m_unoptimizedData;
    }
    SharedPtr<const ShaderProgram> &program(bool optimized)
    {
        return optimized ? m_optimizedProgram : m_unoptimizedProgram;
    }
    ShaderPerformanceMeasurer::Result &result(bool optimized)
    {
        return optimized ? m_optimizedResult : m_unoptimizedResult;
    }

    State m_state;
    ShaderPerformanceMeasurer m_measurer;

    ProgramData m_unoptimizedData;
    ProgramData m_optimizedData;
    SharedPtr<const ShaderProgram> m_unoptimizedProgram;
    SharedPtr<const ShaderProgram> m_optimizedProgram;
    ShaderPerformanceMeasurer::Result m_unoptimizedResult;
    ShaderPerformanceMeasurer::Result m_optimizedResult;
};

void ShaderOptimizationCase::init(void)
{
    const glu::RenderContext &renderCtx = m_context.getRenderContext();
    TestLog &log                        = m_testCtx.getLog();

    m_measurer.logParameters(log);

    for (int ndx = 0; ndx < 2; ndx++)
    {
        const bool optimized = ndx == 1;

        programData(optimized) = generateProgramData(optimized);

        for (int i = 0; i < (int)programData(optimized).attributes.size(); i++)
            DE_ASSERT(programData(optimized).attributes[i].name !=
                      "a_position"); // \note Position attribute is set by m_measurer.

        program(optimized) =
            SharedPtr<const ShaderProgram>(new ShaderProgram(renderCtx, programData(optimized).sources));

        {
            const tcu::ScopedLogSection section(log, optimized ? "OptimizedProgram" : "UnoptimizedProgram",
                                                optimized ? "Hand-optimized program" : "Unoptimized program");
            log << *program(optimized);
        }

        if (!program(optimized)->isOk())
            TCU_FAIL("Shader compilation failed");
    }

    m_state = STATE_INIT_UNOPTIMIZED;
}

ShaderOptimizationCase::IterateResult ShaderOptimizationCase::iterate(void)
{
    TestLog &log = m_testCtx.getLog();

    if (m_state == STATE_INIT_UNOPTIMIZED || m_state == STATE_INIT_OPTIMIZED)
    {
        const bool optimized = m_state == STATE_INIT_OPTIMIZED;
        m_measurer.init(program(optimized)->getProgram(), programData(optimized).attributes, 1);
        m_state = optimized ? STATE_MEASURE_OPTIMIZED : STATE_MEASURE_UNOPTIMIZED;

        return CONTINUE;
    }
    else if (m_state == STATE_MEASURE_UNOPTIMIZED || m_state == STATE_MEASURE_OPTIMIZED)
    {
        m_measurer.iterate();

        if (m_measurer.isFinished())
        {
            const bool optimized = m_state == STATE_MEASURE_OPTIMIZED;
            const tcu::ScopedLogSection section(log, optimized ? "OptimizedResult" : "UnoptimizedResult",
                                                optimized ? "Measurement results for hand-optimized program" :
                                                            "Measurement result for unoptimized program");
            m_measurer.logMeasurementInfo(log);
            result(optimized) = m_measurer.getResult();
            m_measurer.deinit();
            m_state = optimized ? STATE_FINISHED : STATE_INIT_OPTIMIZED;
        }

        return CONTINUE;
    }
    else
    {
        DE_ASSERT(m_state == STATE_FINISHED);

        const float unoptimizedRelevantResult = m_caseShaderType == CASESHADERTYPE_VERTEX ?
                                                    m_unoptimizedResult.megaVertPerSec :
                                                    m_unoptimizedResult.megaFragPerSec;
        const float optimizedRelevantResult   = m_caseShaderType == CASESHADERTYPE_VERTEX ?
                                                    m_optimizedResult.megaVertPerSec :
                                                    m_optimizedResult.megaFragPerSec;
        const char *const relevantResultName  = m_caseShaderType == CASESHADERTYPE_VERTEX ? "vertex" : "fragment";
        const float ratio                     = unoptimizedRelevantResult / optimizedRelevantResult;
        const int handOptimizationGain        = (int)deFloatRound(100.0f / ratio) - 100;

        log << TestLog::Message << "Unoptimized / optimized " << relevantResultName << " performance ratio: " << ratio
            << TestLog::EndMessage;

        if (handOptimizationGain >= 0)
            log << TestLog::Message << "Note: " << handOptimizationGain
                << "% performance gain was achieved with hand-optimized version" << TestLog::EndMessage;
        else
            log << TestLog::Message << "Note: hand-optimization degraded performance by " << -handOptimizationGain
                << "%" << TestLog::EndMessage;

        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::floatToString(ratio, 2).c_str());

        return STOP;
    }
}

class LoopUnrollCase : public ShaderOptimizationCase
{
public:
    enum CaseType
    {
        CASETYPE_INDEPENDENT = 0,
        CASETYPE_DEPENDENT,

        CASETYPE_LAST
    };

    LoopUnrollCase(Context &context, const char *name, const char *description, CaseShaderType caseShaderType,
                   CaseType caseType, int numRepetitions)
        : ShaderOptimizationCase(context, name, description, caseShaderType)
        , m_numRepetitions(numRepetitions)
        , m_caseType(caseType)
    {
    }

protected:
    ProgramData generateProgramData(bool optimized) const
    {
        const string repetition =
            optimized ? repeatIndexedTemplate("\t" + expressionTemplate(m_caseType) + ";\n", m_numRepetitions) :
                        loop(m_numRepetitions, expressionTemplate(m_caseType));

        return defaultProgramData(m_caseShaderType, "\t" + getShaderPrecision(m_caseShaderType) +
                                                        " vec4 valueOrig = value;\n" + repetition);
    }

private:
    const int m_numRepetitions;
    const CaseType m_caseType;

    static inline string expressionTemplate(CaseType caseType)
    {
        switch (caseType)
        {
        case CASETYPE_INDEPENDENT:
            return "value += sin(float(${NDX}+1)*valueOrig)";
        case CASETYPE_DEPENDENT:
            return "value = sin(value)";
        default:
            DE_ASSERT(false);
            return "";
        }
    }

    static inline string loop(int iterations, const string &innerExpr)
    {
        return "\tfor (int i = 0; i < " + toString(iterations) + "; i++)\n\t\t" +
               tcu::StringTemplate(innerExpr).specialize(singleMap("NDX", "i")) + ";\n";
    }
};

class LoopInvariantCodeMotionCase : public ShaderOptimizationCase
{
public:
    LoopInvariantCodeMotionCase(Context &context, const char *name, const char *description,
                                CaseShaderType caseShaderType, int numLoopIterations)
        : ShaderOptimizationCase(context, name, description, caseShaderType)
        , m_numLoopIterations(numLoopIterations)
    {
    }

protected:
    ProgramData generateProgramData(bool optimized) const
    {
        float scale = 0.0f;
        for (int i = 0; i < m_numLoopIterations; i++)
            scale += 3.2f * (float)i + 4.6f;
        scale = 1.0f / scale;

        const string precision  = getShaderPrecision(m_caseShaderType);
        const string statements = optimized ? "    " + precision +
                                                  " vec4 valueOrig = value;\n"
                                                  "    " +
                                                  precision +
                                                  " vec4 y = sin(cos(sin(valueOrig)));\n"
                                                  "    for (int i = 0; i < " +
                                                  toString(m_numLoopIterations) +
                                                  "; i++)\n"
                                                  "    {\n"
                                                  "        " +
                                                  precision +
                                                  " float x = 3.2*float(i) + 4.6;\n"
                                                  "        value += x*y;\n"
                                                  "    }\n"
                                                  "    value *= " +
                                                  toString(scale) + ";\n"

                                              :
                                              "    " + precision +
                                                  " vec4 valueOrig = value;\n"
                                                  "    for (int i = 0; i < " +
                                                  toString(m_numLoopIterations) +
                                                  "; i++)\n"
                                                  "    {\n"
                                                  "        " +
                                                  precision +
                                                  " float x = 3.2*float(i) + 4.6;\n"
                                                  "        " +
                                                  precision +
                                                  " vec4 y = sin(cos(sin(valueOrig)));\n"
                                                  "        value += x*y;\n"
                                                  "    }\n"
                                                  "    value *= " +
                                                  toString(scale) + ";\n";

        return defaultProgramData(m_caseShaderType, statements);
    }

private:
    const int m_numLoopIterations;
};

class FunctionInliningCase : public ShaderOptimizationCase
{
public:
    FunctionInliningCase(Context &context, const char *name, const char *description, CaseShaderType caseShaderType,
                         int callNestingDepth)
        : ShaderOptimizationCase(context, name, description, caseShaderType)
        , m_callNestingDepth(callNestingDepth)
    {
    }

protected:
    ProgramData generateProgramData(bool optimized) const
    {
        const string precision     = getShaderPrecision(m_caseShaderType);
        const string expression    = "value*vec4(0.8, 0.7, 0.6, 0.9)";
        const string maybeFuncDefs = optimized ? "" : funcDefinitions(m_callNestingDepth, precision, expression);
        const string mainValueStatement =
            (optimized ? "\tvalue = " + expression : "\tvalue = func" + toString(m_callNestingDepth - 1) + "(value)") +
            ";\n";

        return defaultProgramData(m_caseShaderType, maybeFuncDefs, mainValueStatement);
    }

private:
    const int m_callNestingDepth;

    static inline string funcDefinitions(int callNestingDepth, const string &precision, const string &expression)
    {
        string result = precision + " vec4 func0 (" + precision + " vec4 value) { return " + expression + "; }\n";

        for (int i = 1; i < callNestingDepth; i++)
            result += precision + " vec4 func" + toString(i) + " (" + precision + " vec4 v) { return func" +
                      toString(i - 1) + "(v); }\n";

        return result;
    }
};

class ConstantPropagationCase : public ShaderOptimizationCase
{
public:
    enum CaseType
    {
        CASETYPE_BUILT_IN_FUNCTIONS = 0,
        CASETYPE_ARRAY,
        CASETYPE_STRUCT,

        CASETYPE_LAST
    };

    ConstantPropagationCase(Context &context, const char *name, const char *description, CaseShaderType caseShaderType,
                            CaseType caseType, bool useConstantExpressionsOnly)
        : ShaderOptimizationCase(context, name, description, caseShaderType)
        , m_caseType(caseType)
        , m_useConstantExpressionsOnly(useConstantExpressionsOnly)
    {
        DE_ASSERT(
            !(m_caseType == CASETYPE_ARRAY &&
              m_useConstantExpressionsOnly)); // \note Would need array constructors, which GLSL ES 1 doesn't have.
    }

protected:
    ProgramData generateProgramData(bool optimized) const
    {
        const bool isVertexCase = m_caseShaderType == CASESHADERTYPE_VERTEX;
        const string precision  = getShaderPrecision(m_caseShaderType);
        const string statements =
            m_caseType == CASETYPE_BUILT_IN_FUNCTIONS ?
                builtinFunctionsCaseStatements(optimized, m_useConstantExpressionsOnly, precision, isVertexCase) :
            m_caseType == CASETYPE_ARRAY ?
                arrayCaseStatements(optimized, precision, isVertexCase) :
            m_caseType == CASETYPE_STRUCT ?
                structCaseStatements(optimized, m_useConstantExpressionsOnly, precision, isVertexCase) :
                deFatalStr("Invalid CaseType");

        return defaultProgramData(m_caseShaderType, statements);
    }

private:
    const CaseType m_caseType;
    const bool m_useConstantExpressionsOnly;

    static inline string builtinFunctionsCaseStatements(bool optimized, bool constantExpressionsOnly,
                                                        const string &precision, bool useHeavierWorkload)
    {
        const string constMaybe = constantExpressionsOnly ? "const " : "";
        const int numSinRows    = useHeavierWorkload ? 12 : 1;

        return optimized ? "    value = vec4(0.4, 0.5, 0.6, 0.7) * value; // NOTE: factor doesn't necessarily match "
                           "the one in unoptimized shader, but shouldn't make a difference performance-wise\n"

                           :
                           "    " + constMaybe + precision +
                               " vec4 a = vec4(sin(0.7), cos(0.2), sin(0.9), abs(-0.5));\n"
                               "    " +
                               constMaybe + precision +
                               " vec4 b = cos(a) + fract(3.0*a.xzzw);\n"
                               "    " +
                               constMaybe +
                               "bvec4 c = bvec4(true, false, true, true);\n"
                               "    " +
                               constMaybe + precision +
                               " vec4 d = exp(b + vec4(c));\n"
                               "    " +
                               constMaybe + precision + " vec4 e0 = inversesqrt(mix(d+a, d+b, a));\n" +
                               repeatIndexedTemplate("    " + constMaybe + precision +
                                                         " vec4 e${NDX} = sin(sin(sin(sin(e${PREV_NDX}))));\n",
                                                     numSinRows, "", 1) +
                               "    " + constMaybe + precision + " vec4 f = abs(e" + toString(numSinRows) + ");\n" +
                               "    value = f*value;\n";
    }

    static inline string arrayCaseStatements(bool optimized, const string &precision, bool useHeavierWorkload)
    {
        const int numSinRows = useHeavierWorkload ? 12 : 1;

        return optimized ?
                   "    value = vec4(0.4, 0.5, 0.6, 0.7) * value; // NOTE: factor doesn't necessarily match the one in "
                   "unoptimized shader, but shouldn't make a difference performance-wise\n"

                   :
                   "    const int arrLen = 4;\n"
                   "    " +
                       precision +
                       " vec4 arr[arrLen];\n"
                       "    arr[0] = vec4(0.1, 0.5, 0.9, 1.3);\n"
                       "    arr[1] = vec4(0.2, 0.6, 1.0, 1.4);\n"
                       "    arr[2] = vec4(0.3, 0.7, 1.1, 1.5);\n"
                       "    arr[3] = vec4(0.4, 0.8, 1.2, 1.6);\n"
                       "    " +
                       precision +
                       " vec4 a = (arr[0] + arr[1] + arr[2] + arr[3]) * 0.25;\n"
                       "    " +
                       precision + " vec4 b0 = cos(sin(a));\n" +
                       repeatIndexedTemplate("    " + precision + " vec4 b${NDX} = sin(sin(sin(sin(b${PREV_NDX}))));\n",
                                             numSinRows, "", 1) +
                       "    " + precision + " vec4 c = abs(b" + toString(numSinRows) + ");\n" +
                       "    value = c*value;\n";
    }

    static inline string structCaseStatements(bool optimized, bool constantExpressionsOnly, const string &precision,
                                              bool useHeavierWorkload)
    {
        const string constMaybe = constantExpressionsOnly ? "const " : "";
        const int numSinRows    = useHeavierWorkload ? 12 : 1;

        return optimized ? "    value = vec4(0.4, 0.5, 0.6, 0.7) * value; // NOTE: factor doesn't necessarily match "
                           "the one in unoptimized shader, but shouldn't make a difference performance-wise\n"

                           :
                           "    struct S\n"
                           "    {\n"
                           "        " +
                               precision +
                               " vec4 a;\n"
                               "        " +
                               precision +
                               " vec4 b;\n"
                               "        " +
                               precision +
                               " vec4 c;\n"
                               "        " +
                               precision +
                               " vec4 d;\n"
                               "    };\n"
                               "\n"
                               "    " +
                               constMaybe +
                               "S s =\n"
                               "        S(vec4(0.1, 0.5, 0.9, 1.3),\n"
                               "          vec4(0.2, 0.6, 1.0, 1.4),\n"
                               "          vec4(0.3, 0.7, 1.1, 1.5),\n"
                               "          vec4(0.4, 0.8, 1.2, 1.6));\n"
                               "    " +
                               constMaybe + precision +
                               " vec4 a = (s.a + s.b + s.c + s.d) * 0.25;\n"
                               "    " +
                               constMaybe + precision + " vec4 b0 = cos(sin(a));\n" +
                               repeatIndexedTemplate("    " + constMaybe + precision +
                                                         " vec4 b${NDX} = sin(sin(sin(sin(b${PREV_NDX}))));\n",
                                                     numSinRows, "", 1) +
                               "    " + constMaybe + precision + " vec4 c = abs(b" + toString(numSinRows) + ");\n" +
                               "    value = c*value;\n";
    }
};

class CommonSubexpressionCase : public ShaderOptimizationCase
{
public:
    enum CaseType
    {
        CASETYPE_SINGLE_STATEMENT = 0,
        CASETYPE_MULTIPLE_STATEMENTS,
        CASETYPE_STATIC_BRANCH,
        CASETYPE_LOOP,

        CASETYPE_LAST
    };

    CommonSubexpressionCase(Context &context, const char *name, const char *description, CaseShaderType caseShaderType,
                            CaseType caseType)
        : ShaderOptimizationCase(context, name, description, caseShaderType)
        , m_caseType(caseType)
    {
    }

protected:
    ProgramData generateProgramData(bool optimized) const
    {
        const bool isVertexCase = m_caseShaderType == CASESHADERTYPE_VERTEX;
        const string precision  = getShaderPrecision(m_caseShaderType);
        const string statements = m_caseType == CASETYPE_SINGLE_STATEMENT ?
                                      singleStatementCaseStatements(optimized, precision, isVertexCase) :
                                  m_caseType == CASETYPE_MULTIPLE_STATEMENTS ?
                                      multipleStatementsCaseStatements(optimized, precision, isVertexCase) :
                                  m_caseType == CASETYPE_STATIC_BRANCH ?
                                      staticBranchCaseStatements(optimized, precision, isVertexCase) :
                                  m_caseType == CASETYPE_LOOP ? loopCaseStatements(optimized, precision, isVertexCase) :
                                                                deFatalStr("Invalid CaseType");

        return defaultProgramData(m_caseShaderType, statements);
    }

private:
    const CaseType m_caseType;

    static inline string singleStatementCaseStatements(bool optimized, const string &precision, bool useHeavierWorkload)
    {
        const int numTopLevelRepeats = useHeavierWorkload ? 4 : 1;

        return optimized ? "    " + precision +
                               " vec4 s = sin(value);\n"
                               "    " +
                               precision +
                               " vec4 cs = cos(s);\n"
                               "    " +
                               precision +
                               " vec4 d = fract(s + cs) + sqrt(s + exp(cs));\n"
                               "    value = " +
                               repeat("d", numTopLevelRepeats, "+") + ";\n"

                           :
                           "    value = " +
                               repeat("fract(sin(value) + cos(sin(value))) + sqrt(sin(value) + exp(cos(sin(value))))",
                                      numTopLevelRepeats, "\n\t      + ") +
                               ";\n";
    }

    static inline string multipleStatementsCaseStatements(bool optimized, const string &precision,
                                                          bool useHeavierWorkload)
    {
        const int numTopLevelRepeats = useHeavierWorkload ? 4 : 2;
        DE_ASSERT(numTopLevelRepeats >= 2);

        return optimized ? "    " + precision +
                               " vec4 a = sin(value) + cos(exp(value));\n"
                               "    " +
                               precision +
                               " vec4 b = cos(cos(a));\n"
                               "    a = fract(exp(sqrt(b)));\n"
                               "\n" +
                               repeat("\tvalue += a*b;\n", numTopLevelRepeats)

                           :
                           repeatIndexedTemplate("    " + precision +
                                                     " vec4 a${NDX} = sin(value) + cos(exp(value));\n"
                                                     "    " +
                                                     precision +
                                                     " vec4 b${NDX} = cos(cos(a${NDX}));\n"
                                                     "    a${NDX} = fract(exp(sqrt(b${NDX})));\n"
                                                     "\n",
                                                 numTopLevelRepeats) +

                               repeatIndexedTemplate("    value += a${NDX}*b${NDX};\n", numTopLevelRepeats);
    }

    static inline string staticBranchCaseStatements(bool optimized, const string &precision, bool useHeavierWorkload)
    {
        const int numTopLevelRepeats = useHeavierWorkload ? 4 : 2;
        DE_ASSERT(numTopLevelRepeats >= 2);

        if (optimized)
        {
            return "    " + precision +
                   " vec4 a = sin(value) + cos(exp(value));\n"
                   "    " +
                   precision +
                   " vec4 b = cos(a);\n"
                   "    b = cos(b);\n"
                   "    a = fract(exp(sqrt(b)));\n"
                   "\n" +
                   repeat("    value += a*b;\n", numTopLevelRepeats);
        }
        else
        {
            string result;

            for (int i = 0; i < numTopLevelRepeats; i++)
            {
                result += "    " + precision + " vec4 a" + toString(i) +
                          " = sin(value) + cos(exp(value));\n"
                          "    " +
                          precision + " vec4 b" + toString(i) + " = cos(a" + toString(i) + ");\n";

                if (i % 3 == 0)
                    result += "    if (1 < 2)\n"
                              "        b" +
                              toString(i) + " = cos(b" + toString(i) + ");\n";
                else if (i % 3 == 1)
                    result += "    b" + toString(i) + " = cos(b" + toString(i) + ");\n";
                else if (i % 3 == 2)
                    result += "    if (2 < 1);\n"
                              "    else\n"
                              "        b" +
                              toString(i) + " = cos(b" + toString(i) + ");\n";
                else
                    DE_ASSERT(false);

                result += "    a" + toString(i) + " = fract(exp(sqrt(b" + toString(i) + ")));\n\n";
            }

            result += repeatIndexedTemplate("    value += a${NDX}*b${NDX};\n", numTopLevelRepeats);

            return result;
        }
    }

    static inline string loopCaseStatements(bool optimized, const string &precision, bool useHeavierWorkload)
    {
        const int numLoopIterations = useHeavierWorkload ? 32 : 4;

        return optimized ? "    " + precision +
                               " vec4 acc = value;\n"
                               "    for (int i = 0; i < " +
                               toString(numLoopIterations) +
                               "; i++)\n"
                               "        acc = sin(acc);\n"
                               "\n"
                               "    value += acc;\n"
                               "    value += acc;\n"

                           :
                           "    " + precision +
                               " vec4 acc0 = value;\n"
                               "    for (int i = 0; i < " +
                               toString(numLoopIterations) +
                               "; i++)\n"
                               "        acc0 = sin(acc0);\n"
                               "\n"
                               "    " +
                               precision +
                               " vec4 acc1 = value;\n"
                               "    for (int i = 0; i < " +
                               toString(numLoopIterations) +
                               "; i++)\n"
                               "        acc1 = sin(acc1);\n"
                               "\n"
                               "    value += acc0;\n"
                               "    value += acc1;\n";
    }
};

class DeadCodeEliminationCase : public ShaderOptimizationCase
{
public:
    enum CaseType
    {
        CASETYPE_DEAD_BRANCH_SIMPLE = 0,
        CASETYPE_DEAD_BRANCH_COMPLEX,
        CASETYPE_DEAD_BRANCH_COMPLEX_NO_CONST,
        CASETYPE_DEAD_BRANCH_FUNC_CALL,
        CASETYPE_UNUSED_VALUE_BASIC,
        CASETYPE_UNUSED_VALUE_LOOP,
        CASETYPE_UNUSED_VALUE_DEAD_BRANCH,
        CASETYPE_UNUSED_VALUE_AFTER_RETURN,
        CASETYPE_UNUSED_VALUE_MUL_ZERO,

        CASETYPE_LAST
    };

    DeadCodeEliminationCase(Context &context, const char *name, const char *description, CaseShaderType caseShaderType,
                            CaseType caseType)
        : ShaderOptimizationCase(context, name, description, caseShaderType)
        , m_caseType(caseType)
    {
    }

protected:
    ProgramData generateProgramData(bool optimized) const
    {
        const bool isVertexCase = m_caseShaderType == CASESHADERTYPE_VERTEX;
        const string precision  = getShaderPrecision(m_caseShaderType);
        const string funcDefs   = m_caseType == CASETYPE_DEAD_BRANCH_FUNC_CALL ?
                                      deadBranchFuncCallCaseFuncDefs(optimized, precision) :
                                  m_caseType == CASETYPE_UNUSED_VALUE_AFTER_RETURN ?
                                      unusedValueAfterReturnCaseFuncDefs(optimized, precision, isVertexCase) :
                                      "";

        const string statements = m_caseType == CASETYPE_DEAD_BRANCH_SIMPLE ?
                                      deadBranchSimpleCaseStatements(optimized, isVertexCase) :
                                  m_caseType == CASETYPE_DEAD_BRANCH_COMPLEX ?
                                      deadBranchComplexCaseStatements(optimized, precision, true, isVertexCase) :
                                  m_caseType == CASETYPE_DEAD_BRANCH_COMPLEX_NO_CONST ?
                                      deadBranchComplexCaseStatements(optimized, precision, false, isVertexCase) :
                                  m_caseType == CASETYPE_DEAD_BRANCH_FUNC_CALL ?
                                      deadBranchFuncCallCaseStatements(optimized, isVertexCase) :
                                  m_caseType == CASETYPE_UNUSED_VALUE_BASIC ?
                                      unusedValueBasicCaseStatements(optimized, precision, isVertexCase) :
                                  m_caseType == CASETYPE_UNUSED_VALUE_LOOP ?
                                      unusedValueLoopCaseStatements(optimized, precision, isVertexCase) :
                                  m_caseType == CASETYPE_UNUSED_VALUE_DEAD_BRANCH ?
                                      unusedValueDeadBranchCaseStatements(optimized, precision, isVertexCase) :
                                  m_caseType == CASETYPE_UNUSED_VALUE_AFTER_RETURN ?
                                      unusedValueAfterReturnCaseStatements() :
                                  m_caseType == CASETYPE_UNUSED_VALUE_MUL_ZERO ?
                                      unusedValueMulZeroCaseStatements(optimized, precision, isVertexCase) :
                                      deFatalStr("Invalid CaseType");

        return defaultProgramData(m_caseShaderType, funcDefs, statements);
    }

private:
    const CaseType m_caseType;

    static inline string deadBranchSimpleCaseStatements(bool optimized, bool useHeavierWorkload)
    {
        const int numLoopIterations = useHeavierWorkload ? 16 : 4;

        return optimized ? "    value = vec4(0.6, 0.7, 0.8, 0.9) * value;\n"

                           :
                           "    value = vec4(0.6, 0.7, 0.8, 0.9) * value;\n"
                           "    if (2 < 1)\n"
                           "    {\n"
                           "        value = cos(exp(sin(value))*log(sqrt(value)));\n"
                           "        for (int i = 0; i < " +
                               toString(numLoopIterations) +
                               "; i++)\n"
                               "            value = sin(value);\n"
                               "    }\n";
    }

    static inline string deadBranchComplexCaseStatements(bool optimized, const string &precision, bool useConst,
                                                         bool useHeavierWorkload)
    {
        const string constMaybe     = useConst ? "const " : "";
        const int numLoopIterations = useHeavierWorkload ? 16 : 4;

        return optimized ? "    value = vec4(0.6, 0.7, 0.8, 0.9) * value;\n"

                           :
                           "    value = vec4(0.6, 0.7, 0.8, 0.9) * value;\n"
                           "    " +
                               constMaybe + precision +
                               " vec4 a = vec4(sin(0.7), cos(0.2), sin(0.9), abs(-0.5));\n"
                               "    " +
                               constMaybe + precision +
                               " vec4 b = cos(a) + fract(3.0*a.xzzw);\n"
                               "    " +
                               constMaybe +
                               "bvec4 c = bvec4(true, false, true, true);\n"
                               "    " +
                               constMaybe + precision +
                               " vec4 d = exp(b + vec4(c));\n"
                               "    " +
                               constMaybe + precision +
                               " vec4 e = 1.8*abs(sin(sin(inversesqrt(mix(d+a, d+b, a)))));\n"
                               "    if (e.x > 1.0)\n"
                               "    {\n"
                               "        value = cos(exp(sin(value))*log(sqrt(value)));\n"
                               "        for (int i = 0; i < " +
                               toString(numLoopIterations) +
                               "; i++)\n"
                               "            value = sin(value);\n"
                               "    }\n";
    }

    static inline string deadBranchFuncCallCaseFuncDefs(bool optimized, const string &precision)
    {
        return optimized ? "" : precision + " float func (" + precision + " float x) { return 2.0*x; }\n";
    }

    static inline string deadBranchFuncCallCaseStatements(bool optimized, bool useHeavierWorkload)
    {
        const int numLoopIterations = useHeavierWorkload ? 16 : 4;

        return optimized ? "    value = vec4(0.6, 0.7, 0.8, 0.9) * value;\n"

                           :
                           "    value = vec4(0.6, 0.7, 0.8, 0.9) * value;\n"
                           "    if (func(0.3) > 1.0)\n"
                           "    {\n"
                           "        value = cos(exp(sin(value))*log(sqrt(value)));\n"
                           "        for (int i = 0; i < " +
                               toString(numLoopIterations) +
                               "; i++)\n"
                               "            value = sin(value);\n"
                               "    }\n";
    }

    static inline string unusedValueBasicCaseStatements(bool optimized, const string &precision,
                                                        bool useHeavierWorkload)
    {
        const int numSinRows = useHeavierWorkload ? 12 : 1;

        return optimized ? "    " + precision +
                               " vec4 used = vec4(0.6, 0.7, 0.8, 0.9) * value;\n"
                               "    value = used;\n"

                           :
                           "    " + precision +
                               " vec4 used = vec4(0.6, 0.7, 0.8, 0.9) * value;\n"
                               "    " +
                               precision + " vec4 unused = cos(exp(sin(value))*log(sqrt(value))) + used;\n" +
                               repeat("    unused = sin(sin(sin(sin(unused))));\n", numSinRows) + "    value = used;\n";
    }

    static inline string unusedValueLoopCaseStatements(bool optimized, const string &precision, bool useHeavierWorkload)
    {
        const int numLoopIterations = useHeavierWorkload ? 16 : 4;

        return optimized ? "    " + precision +
                               " vec4 used = vec4(0.6, 0.7, 0.8, 0.9) * value;\n"
                               "    value = used;\n"

                           :
                           "    " + precision +
                               " vec4 used = vec4(0.6, 0.7, 0.8, 0.9) * value;\n"
                               "    " +
                               precision +
                               " vec4 unused = cos(exp(sin(value))*log(sqrt(value)));\n"
                               "    for (int i = 0; i < " +
                               toString(numLoopIterations) +
                               "; i++)\n"
                               "        unused = sin(unused + used);\n"
                               "    value = used;\n";
    }

    static inline string unusedValueAfterReturnCaseFuncDefs(bool optimized, const string &precision,
                                                            bool useHeavierWorkload)
    {
        const int numSinRows = useHeavierWorkload ? 12 : 1;

        return optimized ? precision + " vec4 func (" + precision +
                               " vec4 v)\n"
                               "{\n"
                               "    " +
                               precision +
                               " vec4 used = vec4(0.6, 0.7, 0.8, 0.9) * v;\n"
                               "    return used;\n"
                               "}\n"

                           :
                           precision + " vec4 func (" + precision +
                               " vec4 v)\n"
                               "{\n"
                               "    " +
                               precision +
                               " vec4 used = vec4(0.6, 0.7, 0.8, 0.9) * v;\n"
                               "    " +
                               precision + " vec4 unused = cos(exp(sin(v))*log(sqrt(v)));\n" +
                               repeat("    unused = sin(sin(sin(sin(unused))));\n", numSinRows) +
                               "    return used;\n"
                               "    used = used*unused;"
                               "    return used;\n"
                               "}\n";
    }

    static inline string unusedValueAfterReturnCaseStatements(void)
    {
        return "    value = func(value);\n";
    }

    static inline string unusedValueDeadBranchCaseStatements(bool optimized, const string &precision,
                                                             bool useHeavierWorkload)
    {
        const int numSinRows = useHeavierWorkload ? 12 : 1;

        return optimized ? "    " + precision +
                               " vec4 used = vec4(0.6, 0.7, 0.8, 0.9) * value;\n"
                               "    value = used;\n"

                           :
                           "    " + precision +
                               " vec4 used = vec4(0.6, 0.7, 0.8, 0.9) * value;\n"
                               "    " +
                               precision + " vec4 unused = cos(exp(sin(value))*log(sqrt(value)));\n" +
                               repeat("    unused = sin(sin(sin(sin(unused))));\n", numSinRows) +
                               "    if (2 < 1)\n"
                               "        used = used*unused;\n"
                               "    value = used;\n";
    }

    static inline string unusedValueMulZeroCaseStatements(bool optimized, const string &precision,
                                                          bool useHeavierWorkload)
    {
        const int numSinRows = useHeavierWorkload ? 12 : 1;

        return optimized ? "    " + precision +
                               " vec4 used = vec4(0.6, 0.7, 0.8, 0.9) * value;\n"
                               "    value = used;\n"

                           :
                           "    " + precision +
                               " vec4 used = vec4(0.6, 0.7, 0.8, 0.9) * value;\n"
                               "    " +
                               precision + " vec4 unused = cos(exp(sin(value))*log(sqrt(value)));\n" +
                               repeat("    unused = sin(sin(sin(sin(unused))));\n", numSinRows) +
                               "    value = used + unused*float(1-1);\n";
    }
};

} // namespace

ShaderOptimizationTests::ShaderOptimizationTests(Context &context)
    : TestCaseGroup(context, "optimization", "Shader Optimization Performance Tests")
{
}

ShaderOptimizationTests::~ShaderOptimizationTests(void)
{
}

void ShaderOptimizationTests::init(void)
{
    TestCaseGroup *const unrollGroup = new TestCaseGroup(m_context, "loop_unrolling", "Loop Unrolling Cases");
    TestCaseGroup *const loopInvariantCodeMotionGroup =
        new TestCaseGroup(m_context, "loop_invariant_code_motion", "Loop-Invariant Code Motion Cases");
    TestCaseGroup *const inlineGroup = new TestCaseGroup(m_context, "function_inlining", "Function Inlining Cases");
    TestCaseGroup *const constantPropagationGroup =
        new TestCaseGroup(m_context, "constant_propagation", "Constant Propagation Cases");
    TestCaseGroup *const commonSubexpressionGroup =
        new TestCaseGroup(m_context, "common_subexpression_elimination", "Common Subexpression Elimination Cases");
    TestCaseGroup *const deadCodeEliminationGroup =
        new TestCaseGroup(m_context, "dead_code_elimination", "Dead Code Elimination Cases");
    addChild(unrollGroup);
    addChild(loopInvariantCodeMotionGroup);
    addChild(inlineGroup);
    addChild(constantPropagationGroup);
    addChild(commonSubexpressionGroup);
    addChild(deadCodeEliminationGroup);

    for (int caseShaderTypeI = 0; caseShaderTypeI < CASESHADERTYPE_LAST; caseShaderTypeI++)
    {
        const CaseShaderType caseShaderType    = (CaseShaderType)caseShaderTypeI;
        const char *const caseShaderTypeSuffix = caseShaderType == CASESHADERTYPE_VERTEX   ? "_vertex" :
                                                 caseShaderType == CASESHADERTYPE_FRAGMENT ? "_fragment" :
                                                                                             nullptr;

        // Loop unrolling cases.

        {
            static const int loopIterationCounts[] = {4, 8, 32};

            for (int caseTypeI = 0; caseTypeI < LoopUnrollCase::CASETYPE_LAST; caseTypeI++)
            {
                const LoopUnrollCase::CaseType caseType = (LoopUnrollCase::CaseType)caseTypeI;
                const string caseTypeName               = caseType == LoopUnrollCase::CASETYPE_INDEPENDENT ?
                                                              "independent_iterations" :
                                                          caseType == LoopUnrollCase::CASETYPE_DEPENDENT ? "dependent_iterations" :
                                                                                                           nullptr;
                const string caseTypeDesc =
                    caseType == LoopUnrollCase::CASETYPE_INDEPENDENT ? "loop iterations don't depend on each other" :
                    caseType == LoopUnrollCase::CASETYPE_DEPENDENT   ? "loop iterations depend on each other" :
                                                                       nullptr;

                for (int loopIterNdx = 0; loopIterNdx < DE_LENGTH_OF_ARRAY(loopIterationCounts); loopIterNdx++)
                {
                    const int loopIterations = loopIterationCounts[loopIterNdx];
                    const string name        = caseTypeName + "_" + toString(loopIterations) + caseShaderTypeSuffix;
                    const string description = toString(loopIterations) + " iterations; " + caseTypeDesc;

                    unrollGroup->addChild(new LoopUnrollCase(m_context, name.c_str(), description.c_str(),
                                                             caseShaderType, caseType, loopIterations));
                }
            }
        }

        // Loop-invariant code motion cases.

        {
            static const int loopIterationCounts[] = {4, 8, 32};

            for (int loopIterNdx = 0; loopIterNdx < DE_LENGTH_OF_ARRAY(loopIterationCounts); loopIterNdx++)
            {
                const int loopIterations = loopIterationCounts[loopIterNdx];
                const string name        = toString(loopIterations) + "_iterations" + caseShaderTypeSuffix;

                loopInvariantCodeMotionGroup->addChild(
                    new LoopInvariantCodeMotionCase(m_context, name.c_str(), "", caseShaderType, loopIterations));
            }
        }

        // Function inlining cases.

        {
            static const int callNestingDepths[] = {4, 8, 32};

            for (int nestDepthNdx = 0; nestDepthNdx < DE_LENGTH_OF_ARRAY(callNestingDepths); nestDepthNdx++)
            {
                const int nestingDepth = callNestingDepths[nestDepthNdx];
                const string name      = toString(nestingDepth) + "_nested" + caseShaderTypeSuffix;

                inlineGroup->addChild(
                    new FunctionInliningCase(m_context, name.c_str(), "", caseShaderType, nestingDepth));
            }
        }

        // Constant propagation cases.

        for (int caseTypeI = 0; caseTypeI < ConstantPropagationCase::CASETYPE_LAST; caseTypeI++)
        {
            const ConstantPropagationCase::CaseType caseType = (ConstantPropagationCase::CaseType)caseTypeI;
            const string caseTypeName = caseType == ConstantPropagationCase::CASETYPE_BUILT_IN_FUNCTIONS ?
                                            "built_in_functions" :
                                        caseType == ConstantPropagationCase::CASETYPE_ARRAY  ? "array" :
                                        caseType == ConstantPropagationCase::CASETYPE_STRUCT ? "struct" :
                                                                                               nullptr;

            for (int constantExpressionsOnlyI = 0; constantExpressionsOnlyI <= 1; constantExpressionsOnlyI++)
            {
                const bool constantExpressionsOnly = constantExpressionsOnlyI != 0;
                const string name = caseTypeName + (constantExpressionsOnly ? "" : "_no_const") + caseShaderTypeSuffix;

                if (caseType == ConstantPropagationCase::CASETYPE_ARRAY &&
                    constantExpressionsOnly) // \note See ConstantPropagationCase's constructor for explanation.
                    continue;

                constantPropagationGroup->addChild(new ConstantPropagationCase(
                    m_context, name.c_str(), "", caseShaderType, caseType, constantExpressionsOnly));
            }
        }

        // Common subexpression cases.

        for (int caseTypeI = 0; caseTypeI < CommonSubexpressionCase::CASETYPE_LAST; caseTypeI++)
        {
            const CommonSubexpressionCase::CaseType caseType = (CommonSubexpressionCase::CaseType)caseTypeI;

            const string caseTypeName =
                caseType == CommonSubexpressionCase::CASETYPE_SINGLE_STATEMENT    ? "single_statement" :
                caseType == CommonSubexpressionCase::CASETYPE_MULTIPLE_STATEMENTS ? "multiple_statements" :
                caseType == CommonSubexpressionCase::CASETYPE_STATIC_BRANCH       ? "static_branch" :
                caseType == CommonSubexpressionCase::CASETYPE_LOOP                ? "loop" :
                                                                                    nullptr;

            const string description = caseType == CommonSubexpressionCase::CASETYPE_SINGLE_STATEMENT ?
                                           "A single statement containing multiple uses of same subexpression" :
                                       caseType == CommonSubexpressionCase::CASETYPE_MULTIPLE_STATEMENTS ?
                                           "Multiple statements performing same computations" :
                                       caseType == CommonSubexpressionCase::CASETYPE_STATIC_BRANCH ?
                                           "Multiple statements including a static conditional" :
                                       caseType == CommonSubexpressionCase::CASETYPE_LOOP ?
                                           "Multiple loops performing the same computations" :
                                           nullptr;

            commonSubexpressionGroup->addChild(
                new CommonSubexpressionCase(m_context, (caseTypeName + caseShaderTypeSuffix).c_str(),
                                            description.c_str(), caseShaderType, caseType));
        }

        // Dead code elimination cases.

        for (int caseTypeI = 0; caseTypeI < DeadCodeEliminationCase::CASETYPE_LAST; caseTypeI++)
        {
            const DeadCodeEliminationCase::CaseType caseType = (DeadCodeEliminationCase::CaseType)caseTypeI;
            const char *const caseTypeName =
                caseType == DeadCodeEliminationCase::CASETYPE_DEAD_BRANCH_SIMPLE  ? "dead_branch_simple" :
                caseType == DeadCodeEliminationCase::CASETYPE_DEAD_BRANCH_COMPLEX ? "dead_branch_complex" :
                caseType == DeadCodeEliminationCase::CASETYPE_DEAD_BRANCH_COMPLEX_NO_CONST ?
                                                                                    "dead_branch_complex_no_const" :
                caseType == DeadCodeEliminationCase::CASETYPE_DEAD_BRANCH_FUNC_CALL     ? "dead_branch_func_call" :
                caseType == DeadCodeEliminationCase::CASETYPE_UNUSED_VALUE_BASIC        ? "unused_value_basic" :
                caseType == DeadCodeEliminationCase::CASETYPE_UNUSED_VALUE_LOOP         ? "unused_value_loop" :
                caseType == DeadCodeEliminationCase::CASETYPE_UNUSED_VALUE_DEAD_BRANCH  ? "unused_value_dead_branch" :
                caseType == DeadCodeEliminationCase::CASETYPE_UNUSED_VALUE_AFTER_RETURN ? "unused_value_after_return" :
                caseType == DeadCodeEliminationCase::CASETYPE_UNUSED_VALUE_MUL_ZERO     ? "unused_value_mul_zero" :
                                                                                          nullptr;

            const char *const caseTypeDescription =
                caseType == DeadCodeEliminationCase::CASETYPE_DEAD_BRANCH_SIMPLE ?
                    "Do computation inside a branch that is never taken (condition is simple false constant "
                    "expression)" :
                caseType == DeadCodeEliminationCase::CASETYPE_DEAD_BRANCH_COMPLEX ?
                    "Do computation inside a branch that is never taken (condition is complex false constant "
                    "expression)" :
                caseType == DeadCodeEliminationCase::CASETYPE_DEAD_BRANCH_COMPLEX_NO_CONST ?
                    "Do computation inside a branch that is never taken (condition is complex false expression, not "
                    "constant expression but still compile-time computable)" :
                caseType == DeadCodeEliminationCase::CASETYPE_DEAD_BRANCH_FUNC_CALL ?
                    "Do computation inside a branch that is never taken (condition is compile-time computable false "
                    "expression containing function call to a simple inlineable function)" :
                caseType == DeadCodeEliminationCase::CASETYPE_UNUSED_VALUE_BASIC ?
                    "Compute a value that is never used even statically" :
                caseType == DeadCodeEliminationCase::CASETYPE_UNUSED_VALUE_LOOP ?
                    "Compute a value, using a loop, that is never used even statically" :
                caseType == DeadCodeEliminationCase::CASETYPE_UNUSED_VALUE_DEAD_BRANCH ?
                    "Compute a value that is used only inside a statically dead branch" :
                caseType == DeadCodeEliminationCase::CASETYPE_UNUSED_VALUE_AFTER_RETURN ?
                    "Compute a value that is used only after a return statement" :
                caseType == DeadCodeEliminationCase::CASETYPE_UNUSED_VALUE_MUL_ZERO ?
                    "Compute a value that is used but multiplied by a zero constant expression" :
                    nullptr;

            deadCodeEliminationGroup->addChild(
                new DeadCodeEliminationCase(m_context, (string() + caseTypeName + caseShaderTypeSuffix).c_str(),
                                            caseTypeDescription, caseShaderType, caseType));
        }
    }
}

} // namespace Performance
} // namespace gles2
} // namespace deqp
