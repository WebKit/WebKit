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
 * \file  glcShaderConstExprTests.cpp
 * \brief Declares shader constant expressions tests.
 */ /*-------------------------------------------------------------------*/

#include <map>
#include "deMath.h"
#include "deSharedPtr.hpp"
#include "glcShaderConstExprTests.hpp"
#include "glsShaderExecUtil.hpp"
#include "gluContextInfo.hpp"
#include "gluShaderUtil.hpp"
#include "tcuFloat.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"

using namespace deqp::gls::ShaderExecUtil;

namespace glcts
{

namespace ShaderConstExpr
{

struct TestParams
{
    const char *name;
    const char *expression;

    glu::DataType inType;
    int minComponents;
    int maxComponents;

    glu::DataType outType;
    union
    {
        float outputFloat;
        int outputInt;
    };
};

struct ShaderExecutorParams
{
    deqp::Context *context;

    std::string caseName;
    std::string source;

    glu::DataType outType;
    union
    {
        float outputFloat;
        int outputInt;
    };
};

template <typename OutputType>
class ExecutorTestCase : public deqp::TestCase
{
  public:
    ExecutorTestCase(deqp::Context &context,
                     const char *name,
                     glu::ShaderType shaderType,
                     const ShaderSpec &shaderSpec,
                     OutputType expectedOutput);
    virtual ~ExecutorTestCase(void);
    virtual tcu::TestNode::IterateResult iterate(void);

  protected:
    void validateOutput(de::SharedPtr<ShaderExecutor> executor);

    glu::ShaderType m_shaderType;
    ShaderSpec m_shaderSpec;
    OutputType m_expectedOutput;
};

template <typename OutputType>
ExecutorTestCase<OutputType>::ExecutorTestCase(deqp::Context &context,
                                               const char *name,
                                               glu::ShaderType shaderType,
                                               const ShaderSpec &shaderSpec,
                                               OutputType expectedOutput)
    : deqp::TestCase(context, name, ""),
      m_shaderType(shaderType),
      m_shaderSpec(shaderSpec),
      m_expectedOutput(expectedOutput)
{}

template <typename OutputType>
ExecutorTestCase<OutputType>::~ExecutorTestCase(void)
{}

template <>
void ExecutorTestCase<float>::validateOutput(de::SharedPtr<ShaderExecutor> executor)
{
    float result        = 0.0f;
    void *const outputs = &result;
    executor->execute(1, DE_NULL, &outputs);

    const float epsilon = 0.01f;
    if (de::abs(m_expectedOutput - result) > epsilon)
    {
        m_context.getTestContext().getLog()
            << tcu::TestLog::Message << "Expected: " << m_expectedOutput << " ("
            << tcu::toHex(tcu::Float32(m_expectedOutput).bits())
            << ") but constant expresion returned: " << result << " ("
            << tcu::toHex(tcu::Float32(result).bits()) << "), used " << epsilon
            << " epsilon for comparison" << tcu::TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return;
    }

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    return;
}

template <>
void ExecutorTestCase<int>::validateOutput(de::SharedPtr<ShaderExecutor> executor)
{
    int result          = 0;
    void *const outputs = &result;
    executor->execute(1, DE_NULL, &outputs);

    if (result == m_expectedOutput)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return;
    }

    m_context.getTestContext().getLog()
        << tcu::TestLog::Message << "Expected: " << m_expectedOutput
        << " but constant expresion returned: " << result << tcu::TestLog::EndMessage;
    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
}

template <typename OutputType>
tcu::TestNode::IterateResult ExecutorTestCase<OutputType>::iterate(void)
{
    de::SharedPtr<ShaderExecutor> executor(
        createExecutor(m_context.getRenderContext(), m_shaderType, m_shaderSpec));

    DE_ASSERT(executor.get());

    executor->log(m_context.getTestContext().getLog());

    try
    {
        if (!executor->isOk())
            TCU_FAIL("Compilation failed");

        executor->useProgram();

        validateOutput(executor);
    }
    catch (const tcu::NotSupportedError &e)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << e.what() << tcu::TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, e.getMessage());
    }
    catch (const tcu::TestError &e)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << e.what() << tcu::TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, e.getMessage());
    }

    return tcu::TestNode::STOP;
}

template <typename OutputType>
void createTestCasesForAllShaderTypes(const ShaderExecutorParams &params,
                                      std::vector<tcu::TestNode *> &outputTests)
{
    DE_ASSERT(params.context);

    deqp::Context &context       = *(params.context);
    glu::ContextType contextType = context.getRenderContext().getType();

    ShaderSpec shaderSpec;
    shaderSpec.version = glu::getContextTypeGLSLVersion(contextType);
    shaderSpec.source  = params.source;
    shaderSpec.outputs.push_back(
        Symbol("out0", glu::VarType(params.outType, glu::PRECISION_HIGHP)));

    // Construct list of shaders for which tests can be created
    std::vector<glu::ShaderType> shaderTypes;

    if (glu::contextSupports(contextType, glu::ApiType::core(4, 3)))
    {
        shaderTypes.push_back(glu::SHADERTYPE_VERTEX);
        shaderTypes.push_back(glu::SHADERTYPE_FRAGMENT);
        shaderTypes.push_back(glu::SHADERTYPE_COMPUTE);
        shaderTypes.push_back(glu::SHADERTYPE_GEOMETRY);
        shaderTypes.push_back(glu::SHADERTYPE_TESSELLATION_CONTROL);
        shaderTypes.push_back(glu::SHADERTYPE_TESSELLATION_EVALUATION);
    }
    else if (glu::contextSupports(contextType, glu::ApiType::es(3, 2)))
    {
        shaderSpec.version = glu::GLSL_VERSION_320_ES;
        shaderTypes.push_back(glu::SHADERTYPE_GEOMETRY);
        shaderTypes.push_back(glu::SHADERTYPE_TESSELLATION_CONTROL);
        shaderTypes.push_back(glu::SHADERTYPE_TESSELLATION_EVALUATION);
    }
    else if (glu::contextSupports(contextType, glu::ApiType::es(3, 1)))
    {
        shaderSpec.version = glu::GLSL_VERSION_310_ES;
        shaderTypes.push_back(glu::SHADERTYPE_COMPUTE);
        shaderTypes.push_back(glu::SHADERTYPE_GEOMETRY);
        shaderTypes.push_back(glu::SHADERTYPE_TESSELLATION_CONTROL);
        shaderTypes.push_back(glu::SHADERTYPE_TESSELLATION_EVALUATION);
    }
    else
    {
        shaderTypes.push_back(glu::SHADERTYPE_VERTEX);
        shaderTypes.push_back(glu::SHADERTYPE_FRAGMENT);
    }

    shaderSpec.globalDeclarations += "precision highp float;\n";

    for (std::size_t typeIndex = 0; typeIndex < shaderTypes.size(); ++typeIndex)
    {
        glu::ShaderType shaderType = shaderTypes[typeIndex];
        std::string caseName(params.caseName + '_' + getShaderTypeName(shaderType));

        outputTests.push_back(
            new ExecutorTestCase<OutputType>(context, caseName.c_str(), shaderType, shaderSpec,
                                             static_cast<OutputType>(params.outputFloat)));
    }
}

void createTests(deqp::Context &context,
                 const TestParams *cases,
                 int numCases,
                 const char *shaderTemplateSrc,
                 const char *casePrefix,
                 std::vector<tcu::TestNode *> &outputTests)
{
    const tcu::StringTemplate shaderTemplate(shaderTemplateSrc);
    const char *componentAccess[] = {"", ".y", ".z", ".w"};

    ShaderExecutorParams shaderExecutorParams;
    shaderExecutorParams.context = &context;

    for (int caseIndex = 0; caseIndex < numCases; caseIndex++)
    {
        const TestParams &testCase   = cases[caseIndex];
        const std::string baseName   = testCase.name;
        const int minComponents      = testCase.minComponents;
        const int maxComponents      = testCase.maxComponents;
        const glu::DataType inType   = testCase.inType;
        const std::string expression = testCase.expression;

        // Check for presence of func(vec, scalar) style specialization,
        // use as gatekeeper for applying said specialization
        const bool alwaysScalar = expression.find("${MT}") != std::string::npos;

        std::map<std::string, std::string> shaderTemplateParams;
        shaderTemplateParams["CASE_BASE_TYPE"] = glu::getDataTypeName(testCase.outType);

        shaderExecutorParams.outType     = testCase.outType;
        shaderExecutorParams.outputFloat = testCase.outputFloat;

        for (int component = minComponents - 1; component < maxComponents; component++)
        {
            // Get type name eg. float, vec2, vec3, vec4 (same for other primitive types)
            glu::DataType dataType = static_cast<glu::DataType>(inType + component);
            std::string typeName   = glu::getDataTypeName(dataType);

            // ${T} => final type, ${MT} => final type but with scalar version usable even when T is
            // a vector
            std::map<std::string, std::string> expressionTemplateParams;
            expressionTemplateParams["T"]  = typeName;
            expressionTemplateParams["MT"] = typeName;

            const tcu::StringTemplate expressionTemplate(expression);

            // Add vector access to expression as needed
            shaderTemplateParams["CASE_EXPRESSION"] =
                expressionTemplate.specialize(expressionTemplateParams) +
                componentAccess[component];

            {
                // Add type to case name if we are generating multiple versions
                shaderExecutorParams.caseName = (casePrefix + baseName);
                if (minComponents != maxComponents)
                    shaderExecutorParams.caseName += ("_" + typeName);

                shaderExecutorParams.source = shaderTemplate.specialize(shaderTemplateParams);
                if (shaderExecutorParams.outType == glu::TYPE_FLOAT)
                    createTestCasesForAllShaderTypes<float>(shaderExecutorParams, outputTests);
                else
                    createTestCasesForAllShaderTypes<int>(shaderExecutorParams, outputTests);
            }

            // Deal with functions that allways accept one ore more scalar parameters even when
            // others are vectors
            if (alwaysScalar && component > 0)
            {
                shaderExecutorParams.caseName =
                    casePrefix + baseName + "_" + typeName + "_" + glu::getDataTypeName(inType);

                expressionTemplateParams["MT"] = glu::getDataTypeName(inType);
                shaderTemplateParams["CASE_EXPRESSION"] =
                    expressionTemplate.specialize(expressionTemplateParams) +
                    componentAccess[component];

                shaderExecutorParams.source = shaderTemplate.specialize(shaderTemplateParams);
                if (shaderExecutorParams.outType == glu::TYPE_FLOAT)
                    createTestCasesForAllShaderTypes<float>(shaderExecutorParams, outputTests);
                else
                    createTestCasesForAllShaderTypes<int>(shaderExecutorParams, outputTests);
            }
        }  // component loop
    }
}

}  // namespace ShaderConstExpr

ShaderConstExprTests::ShaderConstExprTests(deqp::Context &context)
    : deqp::TestCaseGroup(context, "constant_expressions", "Constant expressions")
{}

ShaderConstExprTests::~ShaderConstExprTests(void) {}

void ShaderConstExprTests::init(void)
{
    // Needed for autogenerating shader code for increased component counts
    DE_STATIC_ASSERT(glu::TYPE_FLOAT + 1 == glu::TYPE_FLOAT_VEC2);
    DE_STATIC_ASSERT(glu::TYPE_FLOAT + 2 == glu::TYPE_FLOAT_VEC3);
    DE_STATIC_ASSERT(glu::TYPE_FLOAT + 3 == glu::TYPE_FLOAT_VEC4);

    DE_STATIC_ASSERT(glu::TYPE_INT + 1 == glu::TYPE_INT_VEC2);
    DE_STATIC_ASSERT(glu::TYPE_INT + 2 == glu::TYPE_INT_VEC3);
    DE_STATIC_ASSERT(glu::TYPE_INT + 3 == glu::TYPE_INT_VEC4);

    DE_STATIC_ASSERT(glu::TYPE_UINT + 1 == glu::TYPE_UINT_VEC2);
    DE_STATIC_ASSERT(glu::TYPE_UINT + 2 == glu::TYPE_UINT_VEC3);
    DE_STATIC_ASSERT(glu::TYPE_UINT + 3 == glu::TYPE_UINT_VEC4);

    DE_STATIC_ASSERT(glu::TYPE_BOOL + 1 == glu::TYPE_BOOL_VEC2);
    DE_STATIC_ASSERT(glu::TYPE_BOOL + 2 == glu::TYPE_BOOL_VEC3);
    DE_STATIC_ASSERT(glu::TYPE_BOOL + 3 == glu::TYPE_BOOL_VEC4);

    // ${T} => final type, ${MT} => final type but with scalar version usable even when T is a
    // vector
    const ShaderConstExpr::TestParams baseCases[] = {
        {"radians",
         "radians(${T} (90.0))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_FLOAT,
         {deFloatRadians(90.0f)}},
        {"degrees",
         "degrees(${T} (2.0))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_FLOAT,
         {deFloatDegrees(2.0f)}},
        {"sin", "sin(${T} (3.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, {deFloatSin(3.0f)}},
        {"cos", "cos(${T} (3.2))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, {deFloatCos(3.2f)}},
        {"asin", "asin(${T} (0.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, {deFloatAsin(0.0f)}},
        {"acos", "acos(${T} (1.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, {deFloatAcos(1.0f)}},
        {"pow",
         "pow(${T} (1.7), ${T} (3.5))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_FLOAT,
         {deFloatPow(1.7f, 3.5f)}},
        {"exp", "exp(${T} (4.2))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, {deFloatExp(4.2f)}},
        {"log", "log(${T} (42.12))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, {deFloatLog(42.12f)}},
        {"exp2", "exp2(${T} (6.7))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, {deFloatExp2(6.7f)}},
        {"log2",
         "log2(${T} (100.0))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_FLOAT,
         {deFloatLog2(100.0f)}},
        {"sqrt", "sqrt(${T} (10.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, {deFloatSqrt(10.0f)}},
        {"inversesqrt",
         "inversesqrt(${T} (10.0))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_FLOAT,
         {deFloatRsq(10.0f)}},
        {"abs", "abs(${T} (-42))", glu::TYPE_INT, 1, 4, glu::TYPE_INT, {42}},
        {"sign", "sign(${T} (-18.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, {-1.0f}},
        {"floor",
         "floor(${T} (37.3))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_FLOAT,
         {deFloatFloor(37.3f)}},
        {"trunc", "trunc(${T} (-1.8))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, {-1.0f}},
        {"round", "round(${T} (42.1))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, {42.0f}},
        {"ceil", "ceil(${T} (82.2))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, {deFloatCeil(82.2f)}},
        {"mod",
         "mod(${T} (87.65), ${MT} (3.7))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_FLOAT,
         {deFloatMod(87.65f, 3.7f)}},
        {"min", "min(${T} (12.3), ${MT} (32.1))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, {12.3f}},
        {"max", "max(${T} (12.3), ${MT} (32.1))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, {32.1f}},
        {"clamp",
         "clamp(${T} (42.1),    ${MT} (10.0), ${MT} (15.0))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_FLOAT,
         {15.0f}},
        {"length_float", "length(1.0)", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, {1.0f}},
        {"length_vec2",
         "length(vec2(1.0))",
         glu::TYPE_FLOAT,
         1,
         1,
         glu::TYPE_FLOAT,
         {deFloatSqrt(2.0f)}},
        {"length_vec3",
         "length(vec3(1.0))",
         glu::TYPE_FLOAT,
         1,
         1,
         glu::TYPE_FLOAT,
         {deFloatSqrt(3.0f)}},
        {"length_vec4",
         "length(vec4(1.0))",
         glu::TYPE_FLOAT,
         1,
         1,
         glu::TYPE_FLOAT,
         {deFloatSqrt(4.0f)}},
        {"dot_float", "dot(1.0, 1.0)", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, {1.0f}},
        {"dot_vec2", "dot(vec2(1.0), vec2(1.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, {2.0f}},
        {"dot_vec3", "dot(vec3(1.0), vec3(1.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, {3.0f}},
        {"dot_vec4", "dot(vec4(1.0), vec4(1.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, {4.0f}},
        {"normalize_float", "normalize(1.0)", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, {1.0f}},
        {"normalize_vec2",
         "normalize(vec2(1.0)).x",
         glu::TYPE_FLOAT,
         1,
         1,
         glu::TYPE_FLOAT,
         {deFloatRsq(2.0f)}},
        {"normalize_vec3",
         "normalize(vec3(1.0)).x",
         glu::TYPE_FLOAT,
         1,
         1,
         glu::TYPE_FLOAT,
         {deFloatRsq(3.0f)}},
        {"normalize_vec4",
         "normalize(vec4(1.0)).x",
         glu::TYPE_FLOAT,
         1,
         1,
         glu::TYPE_FLOAT,
         {deFloatRsq(4.0f)}},
    };

    const ShaderConstExpr::TestParams arrayCases[] = {
        {"radians",
         "radians(${T} (60.0))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_INT,
         {deFloatRadians(60.0f)}},
        {"degrees",
         "degrees(${T} (0.11))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_INT,
         {deFloatDegrees(0.11f)}},
        {"sin",
         "${T} (1.0) + sin(${T} (0.7))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_INT,
         {1.0f + deFloatSin(0.7f)}},
        {"cos",
         "${T} (1.0) + cos(${T} (0.7))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_INT,
         {1.0f + deFloatCos(0.7f)}},
        {"asin", "asin(${T} (0.9))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_INT, {deFloatAsin(0.9f)}},
        {"acos", "acos(${T} (-0.5))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_INT, {deFloatAcos(-0.5f)}},
        {"pow",
         "pow(${T} (2.0), ${T} (2.0))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_INT,
         {deFloatPow(2.0f, 2.0f)}},
        {"exp", "exp(${T} (1.2))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_INT, {deFloatExp(1.2f)}},
        {"log", "log(${T} (8.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_INT, {deFloatLog(8.0f)}},
        {"exp2", "exp2(${T} (2.1))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_INT, {deFloatExp2(2.1f)}},
        {"log2", "log2(${T} (9.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_INT, {deFloatLog2(9.0)}},
        {"sqrt", "sqrt(${T} (4.5))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_INT, {deFloatSqrt(4.5f)}},
        {"inversesqrt",
         "inversesqrt(${T} (0.26))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_INT,
         {deFloatRsq(0.26f)}},
        {"abs", "abs(${T} (-2))", glu::TYPE_INT, 1, 4, glu::TYPE_INT, {2}},
        {"sign", "sign(${T} (18.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_INT, {deFloatSign(18.0f)}},
        {"floor", "floor(${T} (3.3))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_INT, {deFloatFloor(3.3f)}},
        {"trunc", "trunc(${T} (2.8))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_INT, {2}},
        {"round", "round(${T} (2.2))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_INT, {deFloatRound(2.2f)}},
        {"ceil", "ceil(${T} (2.2))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_INT, {deFloatCeil(2.2f)}},
        {"mod",
         "mod(${T} (7.1), ${MT} (4.0))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_INT,
         {deFloatMod(7.1f, 4.0f)}},
        {"min",
         "min(${T} (2.3), ${MT} (3.1))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_INT,
         {deFloatMin(2.3f, 3.1f)}},
        {"max",
         "max(${T} (2.3), ${MT} (3.1))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_INT,
         {deFloatMax(2.3f, 3.1f)}},
        {"clamp",
         "clamp(${T} (4.1), ${MT} (2.1), ${MT} (3.1))",
         glu::TYPE_FLOAT,
         1,
         4,
         glu::TYPE_INT,
         {3}},
        {"length_float", "length(2.1)", glu::TYPE_FLOAT, 1, 1, glu::TYPE_INT, {2}},
        {"length_vec2",
         "length(vec2(1.0))",
         glu::TYPE_FLOAT,
         1,
         1,
         glu::TYPE_INT,
         {deFloatSqrt(2.0f)}},
        {"length_vec3",
         "length(vec3(1.0))",
         glu::TYPE_FLOAT,
         1,
         1,
         glu::TYPE_INT,
         {deFloatSqrt(3.0f)}},
        {"length_vec4",
         "length(vec4(1.0))",
         glu::TYPE_FLOAT,
         1,
         1,
         glu::TYPE_INT,
         {deFloatSqrt(4.0f)}},
        {"dot_float", "dot(1.0, 1.0)", glu::TYPE_FLOAT, 1, 1, glu::TYPE_INT, {1}},
        {"dot_vec2", "dot(vec2(1.0), vec2(1.01))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_INT, {2}},
        {"dot_vec3", "dot(vec3(1.0), vec3(1.1))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_INT, {3}},
        {"dot_vec4", "dot(vec4(1.0), vec4(1.1))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_INT, {4}},
        {"normalize_float",
         "${T} (1.0) + normalize(2.0)",
         glu::TYPE_FLOAT,
         1,
         1,
         glu::TYPE_INT,
         {2}},
        {"normalize_vec2",
         "${T} (1.0) + normalize(vec2(1.0)).x",
         glu::TYPE_FLOAT,
         1,
         1,
         glu::TYPE_INT,
         {1.0f + deFloatRsq(2.0f)}},
        {"normalize_vec3",
         "${T} (1.0) + normalize(vec3(1.0)).x",
         glu::TYPE_FLOAT,
         1,
         1,
         glu::TYPE_INT,
         {1.0f + deFloatRsq(3.0f)}},
        {"normalize_vec4",
         "${T} (1.0) + normalize(vec4(1.0)).x",
         glu::TYPE_FLOAT,
         1,
         1,
         glu::TYPE_INT,
         {1.0f + deFloatRsq(4.0f)}},
    };

    const char *basicShaderTemplate =
        "const ${CASE_BASE_TYPE} cval = ${CASE_EXPRESSION};\n"
        "out0 = cval;\n";

    std::vector<tcu::TestNode *> children;
    ShaderConstExpr::createTests(m_context, baseCases, DE_LENGTH_OF_ARRAY(baseCases),
                                 basicShaderTemplate, "basic_", children);

    const char *arrayShaderTemplate =
        "float array[int(${CASE_EXPRESSION})];\n"
        "out0 = array.length();\n";

    ShaderConstExpr::createTests(m_context, arrayCases, DE_LENGTH_OF_ARRAY(arrayCases),
                                 arrayShaderTemplate, "array_", children);

    for (std::size_t i = 0; i < children.size(); i++)
        addChild(children[i]);
}

}  // namespace glcts
