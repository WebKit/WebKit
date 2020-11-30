/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
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
 */ /*!
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

#include "deSharedPtr.hpp"
#include "glcShaderMacroTests.hpp"
#include "glsShaderExecUtil.hpp"
#include "gluContextInfo.hpp"
#include "tcuTestLog.hpp"

namespace glcts
{

using tcu::TestLog;
using namespace deqp::gls::ShaderExecUtil;

class ExecutorTestCase : public deqp::TestCase
{
  public:
    ExecutorTestCase(deqp::Context &context,
                     const char *name,
                     glu::ShaderType shaderType,
                     const ShaderSpec &shaderSpec,
                     int expectedOutput);
    virtual ~ExecutorTestCase(void);
    virtual tcu::TestNode::IterateResult iterate(void);

  protected:
    glu::ShaderType m_shaderType;
    ShaderSpec m_shaderSpec;
    int m_expectedOutput;
};

ExecutorTestCase::ExecutorTestCase(deqp::Context &context,
                                   const char *name,
                                   glu::ShaderType shaderType,
                                   const ShaderSpec &shaderSpec,
                                   int expectedOutput)
    : deqp::TestCase(context, name, ""),
      m_shaderType(shaderType),
      m_shaderSpec(shaderSpec),
      m_expectedOutput(expectedOutput)
{}

ExecutorTestCase::~ExecutorTestCase(void) {}

tcu::TestNode::IterateResult ExecutorTestCase::iterate(void)
{
    de::SharedPtr<ShaderExecutor> executor(
        createExecutor(m_context.getRenderContext(), m_shaderType, m_shaderSpec));

    DE_ASSERT(executor.get());

    executor->log(m_context.getTestContext().getLog());

    if (!executor->isOk())
        TCU_FAIL("Compilation failed");

    executor->useProgram();

    int result          = 0;
    void *const outputs = &result;
    executor->execute(1, DE_NULL, &outputs);

    if (m_expectedOutput == result)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return tcu::TestNode::STOP;
    }

    m_context.getTestContext().getLog()
        << tcu::TestLog::Message << "Expected: " << m_expectedOutput
        << " but test returned: " << result << tcu::TestLog::EndMessage;
    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");

    return tcu::TestNode::STOP;
}

ShaderMacroTests::ShaderMacroTests(deqp::Context &context)
    : TestCaseGroup(context, "shader_macros", "Shader Macro tests")
{}

ShaderMacroTests::~ShaderMacroTests() {}

void ShaderMacroTests::init(void)
{
    const char *fragmentPrecisionShaderTemplate =
        "out0 = 0;\n"
        "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
        "out0 = 1;\n"
        "#endif\n";

    glu::ContextType contextType = m_context.getRenderContext().getType();

    ShaderSpec shaderSpec;
    shaderSpec.version = glu::getContextTypeGLSLVersion(contextType);
    shaderSpec.source  = fragmentPrecisionShaderTemplate;
    shaderSpec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_INT, glu::PRECISION_HIGHP)));

    std::vector<glu::ShaderType> shaderTypes;
    shaderTypes.push_back(glu::SHADERTYPE_VERTEX);
    shaderTypes.push_back(glu::SHADERTYPE_FRAGMENT);

    if (glu::contextSupports(contextType, glu::ApiType::es(3, 2)))
    {
        shaderSpec.version = glu::GLSL_VERSION_320_ES;
        shaderTypes.push_back(glu::SHADERTYPE_GEOMETRY);
        shaderTypes.push_back(glu::SHADERTYPE_TESSELLATION_CONTROL);
        shaderTypes.push_back(glu::SHADERTYPE_TESSELLATION_EVALUATION);
    }
    else if (glu::contextSupports(contextType, glu::ApiType::es(3, 1)))
    {
        shaderSpec.version = glu::GLSL_VERSION_310_ES;
        shaderTypes.push_back(glu::SHADERTYPE_GEOMETRY);
        shaderTypes.push_back(glu::SHADERTYPE_TESSELLATION_CONTROL);
        shaderTypes.push_back(glu::SHADERTYPE_TESSELLATION_EVALUATION);
    }

    for (std::size_t typeIndex = 0; typeIndex < shaderTypes.size(); ++typeIndex)
    {
        glu::ShaderType shaderType = shaderTypes[typeIndex];
        std::string caseName("fragment_precision_high_");
        caseName += getShaderTypeName(shaderType);
        addChild(new ExecutorTestCase(m_context, caseName.c_str(), shaderType, shaderSpec, 1));
    }
}

}  // namespace glcts
