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
 * \brief Long shader compilation stress tests
 *//*--------------------------------------------------------------------*/

#include "es3sLongShaderTests.hpp"

#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deString.h"
#include "tcuTestLog.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include <string>
#include <set>
#include <map>
#include <cmath>

using tcu::TestLog;

namespace deqp
{
namespace gles3
{
namespace Stress
{

namespace
{

enum LongShaderCaseFlags
{
    CASE_REQUIRE_LINK_STATUS_OK = 1
};

const char *getConstVertShaderSource(void)
{
    const char *const src = "#version 300 es\n"
                            "void main ()\n"
                            "{\n"
                            "    gl_Position = vec4(0.0);\n"
                            "}\n";

    return src;
}

const char *getConstFragShaderSource(void)
{
    const char *const src = "#version 300 es\n"
                            "layout(location = 0) out mediump vec4 o_fragColor;\n"
                            "void main ()\n"
                            "{\n"
                            "    o_fragColor = vec4(0.0);\n"
                            "}\n";

    return src;
}

const char *getConstShaderSource(const glu::ShaderType shaderType)
{
    DE_ASSERT(shaderType == glu::SHADERTYPE_VERTEX || shaderType == glu::SHADERTYPE_FRAGMENT);

    if (shaderType == glu::SHADERTYPE_VERTEX)
        return getConstVertShaderSource();
    else
        return getConstFragShaderSource();
}

typedef std::set<std::string> ShaderScope;

const char variableNamePrefixChars[] = "abcdefghijklmnopqrstuvwxyz";

class NameGenerator
{
public:
    NameGenerator(void) : m_scopeIndices(1, 0), m_currentScopeDepth(1), m_variableIndex(0)
    {
    }

    void beginScope(void)
    {
        m_currentScopeDepth++;

        if (m_scopeIndices.size() < (size_t)m_currentScopeDepth)
            m_scopeIndices.push_back(0);
        else
            m_scopeIndices[m_currentScopeDepth - 1]++;

        m_variableIndex = 0;
    }

    void endScope(void)
    {
        DE_ASSERT(m_currentScopeDepth > 1);

        m_currentScopeDepth--;
    }

    std::string makePrefix(void)
    {
        std::string prefix;

        for (int ndx = 0; ndx < m_currentScopeDepth; ndx++)
        {
            const int scopeIndex = m_scopeIndices[m_currentScopeDepth - 1];

            DE_ASSERT(scopeIndex < DE_LENGTH_OF_ARRAY(variableNamePrefixChars));

            prefix += variableNamePrefixChars[scopeIndex];
        }

        return prefix;
    }

    std::string next(void)
    {
        m_variableIndex++;

        return makePrefix() + de::toString(m_variableIndex);
    }

    void makeNames(ShaderScope &scope, const uint32_t count)
    {
        for (uint32_t ndx = 0; ndx < count; ndx++)
            scope.insert(next());
    }

private:
    std::vector<int> m_scopeIndices;
    int m_currentScopeDepth;
    int m_variableIndex;
};

struct LongShaderSpec
{
    glu::ShaderType shaderType;
    uint32_t opsTotal;

    uint32_t variablesPerBlock;
    uint32_t opsPerExpression;

    LongShaderSpec(const glu::ShaderType shaderTypeInit, const uint32_t opsTotalInit)
        : shaderType(shaderTypeInit)
        , opsTotal(opsTotalInit)
        , variablesPerBlock(deMaxu32(10, (uint32_t)std::floor(std::sqrt((double)opsTotal))))
        , opsPerExpression(deMinu32(10, variablesPerBlock / 2))
    {
    }
};

// Generator for long test shaders

class LongShaderGenerator
{
public:
    LongShaderGenerator(de::Random &rnd, const LongShaderSpec &spec);

    glu::ShaderSource getSource(void);

private:
    de::Random m_rnd;
    const LongShaderSpec m_spec;

    NameGenerator m_nameGen;

    std::vector<std::string> m_varNames;
    std::vector<ShaderScope> m_scopes;

    std::string m_source;

    void generateSource(void);

    std::string getRandomVariableName(void);
    std::string getShaderOutputName(void);
    std::string makeExpression(const std::vector<std::string> &varNames, const int numOps);

    void addIndent(void);
    void addLine(const std::string &text);

    void beginBlock(void);
    void endBlock(void);
};

LongShaderGenerator::LongShaderGenerator(de::Random &rnd, const LongShaderSpec &spec) : m_rnd(rnd), m_spec(spec)
{
    DE_ASSERT(m_spec.shaderType == glu::SHADERTYPE_VERTEX || m_spec.shaderType == glu::SHADERTYPE_FRAGMENT);
}

glu::ShaderSource LongShaderGenerator::getSource(void)
{
    if (m_source.empty())
        generateSource();

    return glu::ShaderSource(m_spec.shaderType, m_source);
}

void LongShaderGenerator::generateSource(void)
{
    uint32_t currentOpsTotal = 0;

    m_source.clear();

    addLine("#version 300 es");

    if (m_spec.shaderType == glu::SHADERTYPE_FRAGMENT)
        addLine("layout(location = 0) out mediump vec4 o_fragColor;");

    addLine("void main (void)");
    beginBlock();

    while (currentOpsTotal < m_spec.opsTotal)
    {
        const bool isLast    = (m_spec.opsTotal <= (currentOpsTotal + m_spec.opsPerExpression));
        const int numOps     = isLast ? (m_spec.opsTotal - currentOpsTotal) : m_spec.opsPerExpression;
        const size_t numVars = numOps + 1;

        const std::string outName = isLast ? getShaderOutputName() : getRandomVariableName();
        std::vector<std::string> inNames(numVars);

        DE_ASSERT(numVars < m_varNames.size());
        m_rnd.choose(m_varNames.begin(), m_varNames.end(), inNames.begin(), (int)numVars);

        {
            std::string expr = makeExpression(inNames, numOps);

            if (isLast)
                addLine(outName + " = vec4(" + expr + ");");
            else
                addLine(outName + " = " + expr + ";");
        }

        currentOpsTotal += numOps;
    }

    while (!m_scopes.empty())
        endBlock();
}

std::string LongShaderGenerator::getRandomVariableName(void)
{
    return m_rnd.choose<std::string>(m_varNames.begin(), m_varNames.end());
}

std::string LongShaderGenerator::getShaderOutputName(void)
{
    return (m_spec.shaderType == glu::SHADERTYPE_VERTEX) ? "gl_Position" : "o_fragColor";
}

std::string LongShaderGenerator::makeExpression(const std::vector<std::string> &varNames, const int numOps)
{
    const std::string operators = "+-*/";
    std::string expr;

    DE_ASSERT(varNames.size() > (size_t)numOps);

    expr = varNames[0];

    for (int ndx = 1; ndx <= numOps; ndx++)
    {
        const std::string op      = std::string("") + m_rnd.choose<char>(operators.begin(), operators.end());
        const std::string varName = varNames[ndx];

        expr += " " + op + " " + varName;
    }

    return expr;
}

void LongShaderGenerator::addIndent(void)
{
    m_source += std::string(m_scopes.size(), '\t');
}

void LongShaderGenerator::addLine(const std::string &text)
{
    addIndent();
    m_source += text + "\n";
}

void LongShaderGenerator::beginBlock(void)
{
    ShaderScope scope;

    addLine("{");

    m_nameGen.beginScope();
    m_nameGen.makeNames(scope, m_spec.variablesPerBlock);

    m_scopes.push_back(scope);

    for (ShaderScope::const_iterator nameIter = scope.begin(); nameIter != scope.end(); nameIter++)
    {
        const std::string varName = *nameIter;
        const float varValue      = m_rnd.getFloat();

        addLine("mediump float " + varName + " = " + de::floatToString(varValue, 5) + "f;");
        m_varNames.push_back(varName);
    }
}

void LongShaderGenerator::endBlock(void)
{
    ShaderScope &scope = *(m_scopes.end() - 1);

    DE_ASSERT(!m_scopes.empty());

    m_varNames.erase((m_varNames.begin() + (m_varNames.size() - scope.size())), m_varNames.end());

    m_nameGen.endScope();
    m_scopes.pop_back();

    addLine("}");
}

} // namespace

// Stress test case for compilation of large shaders

class LongShaderCompileStressCase : public TestCase
{
public:
    LongShaderCompileStressCase(Context &context, const char *name, const char *desc, const LongShaderSpec &caseSpec,
                                const uint32_t flags);
    virtual ~LongShaderCompileStressCase(void);

    void init(void);

    IterateResult iterate(void);

    void verify(const glu::ShaderProgram &program);

private:
    const glu::ShaderType m_shaderType;
    const uint32_t m_flags;
    de::Random m_rnd;
    LongShaderGenerator m_gen;
};

LongShaderCompileStressCase::LongShaderCompileStressCase(Context &context, const char *name, const char *desc,
                                                         const LongShaderSpec &caseSpec, const uint32_t flags)
    : TestCase(context, name, desc)
    , m_shaderType(caseSpec.shaderType)
    , m_flags(flags)
    , m_rnd(deStringHash(name) ^ 0xac9c91d)
    , m_gen(m_rnd, caseSpec)
{
    DE_ASSERT(m_shaderType == glu::SHADERTYPE_VERTEX || m_shaderType == glu::SHADERTYPE_FRAGMENT);
}

LongShaderCompileStressCase::~LongShaderCompileStressCase(void)
{
}

void LongShaderCompileStressCase::init(void)
{
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
}

tcu::TestCase::IterateResult LongShaderCompileStressCase::iterate(void)
{
    tcu::TestLog &log = m_testCtx.getLog();
    const glu::ShaderType otherShader =
        (m_shaderType == glu::SHADERTYPE_VERTEX) ? glu::SHADERTYPE_FRAGMENT : glu::SHADERTYPE_VERTEX;
    glu::ProgramSources sources;

    sources << m_gen.getSource();
    sources << glu::ShaderSource(otherShader, getConstShaderSource(otherShader));

    {
        glu::ShaderProgram program(m_context.getRenderContext(), sources);

        verify(program);

        log << program;
    }

    return STOP;
}

void LongShaderCompileStressCase::verify(const glu::ShaderProgram &program)
{
    tcu::TestLog &log           = m_testCtx.getLog();
    const glw::Functions &gl    = m_context.getRenderContext().getFunctions();
    const bool isStrict         = (m_flags & CASE_REQUIRE_LINK_STATUS_OK) != 0;
    const glw::GLenum errorCode = gl.getError();

    if (isStrict && !program.isOk())
    {
        log << TestLog::Message << "Fail, expected program to compile and link successfully." << TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Linking failed");
    }

    if (program.isOk() && (errorCode != GL_NO_ERROR))
    {
        log << TestLog::Message << "Fail, program status OK but a GL error was received (" << errorCode << ")."
            << TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Conflicting status");
    }
    else if ((errorCode != GL_NO_ERROR) && (errorCode != GL_OUT_OF_MEMORY))
    {
        log << TestLog::Message << "Fail, expected GL_NO_ERROR or GL_OUT_OF_MEMORY, received " << errorCode << "."
            << TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Unexpected GL error");
    }
}

LongShaderTests::LongShaderTests(Context &testCtx)
    : TestCaseGroup(testCtx, "long_shaders", "Long shader compilation stress tests")
{
}

LongShaderTests::~LongShaderTests(void)
{
}

void LongShaderTests::init(void)
{
    const uint32_t requireLinkOkMaxOps = 1000;

    const uint32_t caseOpCounts[] = {100, 1000, 10000, 100000};

    for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(caseOpCounts); caseNdx++)
    {
        for (int shaderTypeInt = 0; shaderTypeInt < 2; shaderTypeInt++)
        {
            const glu::ShaderType shaderType = (shaderTypeInt == 0) ? glu::SHADERTYPE_VERTEX : glu::SHADERTYPE_FRAGMENT;
            const uint32_t opCount           = caseOpCounts[caseNdx];
            const uint32_t flags             = (opCount <= requireLinkOkMaxOps) ? CASE_REQUIRE_LINK_STATUS_OK : 0;

            const std::string name = de::toString(opCount) + "_operations_" + glu::getShaderTypeName(shaderType);
            const std::string desc = std::string("Compile ") + glu::getShaderTypeName(shaderType) + " shader with " +
                                     de::toString(opCount) + " operations";

            LongShaderSpec caseSpec(shaderType, opCount);

            addChild(new LongShaderCompileStressCase(m_context, name.c_str(), desc.c_str(), caseSpec, flags));
        }
    }
}

} // namespace Stress
} // namespace gles3
} // namespace deqp
