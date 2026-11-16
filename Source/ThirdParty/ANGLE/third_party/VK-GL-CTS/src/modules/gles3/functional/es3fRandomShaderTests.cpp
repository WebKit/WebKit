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
 * \brief Random shader tests.
 *//*--------------------------------------------------------------------*/

#include "es3fRandomShaderTests.hpp"
#include "glsRandomShaderCase.hpp"
#include "deString.h"
#include "deStringUtil.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

namespace
{

gls::RandomShaderCase *createRandomShaderCase(Context &context, const char *description,
                                              const rsg::ProgramParameters &baseParams, uint32_t seed, bool vertex,
                                              bool fragment)
{
    rsg::ProgramParameters params = baseParams;

    params.version                      = rsg::VERSION_300;
    params.seed                         = seed;
    params.vertexParameters.randomize   = vertex;
    params.fragmentParameters.randomize = fragment;

    return new gls::RandomShaderCase(context.getTestContext(), context.getRenderContext(), de::toString(seed).c_str(),
                                     description, params);
}

class BasicExpressionGroup : public TestCaseGroup
{
public:
    BasicExpressionGroup(Context &context) : TestCaseGroup(context, "basic_expression", "Basic arithmetic expressions")
    {
    }

    void init(void)
    {
        rsg::ProgramParameters params;

        tcu::TestCaseGroup *vertexGroup = new tcu::TestCaseGroup(m_testCtx, "vertex", "Vertex-only tests");
        addChild(vertexGroup);

        tcu::TestCaseGroup *fragmentGroup = new tcu::TestCaseGroup(m_testCtx, "fragment", "Fragment-only tests");
        addChild(fragmentGroup);

        tcu::TestCaseGroup *combinedGroup = new tcu::TestCaseGroup(m_testCtx, "combined", "Combined tests");
        addChild(combinedGroup);

        for (int seed = 0; seed < 100; seed++)
        {
            vertexGroup->addChild(
                createRandomShaderCase(m_context, "Random expressions in vertex shader", params, seed, true, false));
            fragmentGroup->addChild(
                createRandomShaderCase(m_context, "Random expressions in fragment shader", params, seed, false, true));
            combinedGroup->addChild(createRandomShaderCase(
                m_context, "Random expressions in vertex and fragment shaders", params, seed, true, true));
        }
    }
};

class ScalarConversionGroup : public TestCaseGroup
{
public:
    ScalarConversionGroup(Context &context) : TestCaseGroup(context, "scalar_conversion", "Scalar conversions")
    {
    }

    void init(void)
    {
        rsg::ProgramParameters params;
        params.useScalarConversions = true;

        tcu::TestCaseGroup *vertexGroup = new tcu::TestCaseGroup(m_testCtx, "vertex", "Vertex-only tests");
        addChild(vertexGroup);

        tcu::TestCaseGroup *fragmentGroup = new tcu::TestCaseGroup(m_testCtx, "fragment", "Fragment-only tests");
        addChild(fragmentGroup);

        tcu::TestCaseGroup *combinedGroup = new tcu::TestCaseGroup(m_testCtx, "combined", "Combined tests");
        addChild(combinedGroup);

        for (int seed = 0; seed < 100; seed++)
        {
            vertexGroup->addChild(
                createRandomShaderCase(m_context, "Scalar conversions in vertex shader", params, seed, true, false));
            fragmentGroup->addChild(
                createRandomShaderCase(m_context, "Scalar conversions in fragment shader", params, seed, false, true));
            combinedGroup->addChild(createRandomShaderCase(
                m_context, "Scalar conversions in vertex and fragment shaders", params, seed, true, true));
        }
    }
};

class SwizzleGroup : public TestCaseGroup
{
public:
    SwizzleGroup(Context &context) : TestCaseGroup(context, "swizzle", "Vector swizzles")
    {
    }

    void init(void)
    {
        rsg::ProgramParameters params;
        params.useScalarConversions = true;
        params.useSwizzle           = true;

        tcu::TestCaseGroup *vertexGroup = new tcu::TestCaseGroup(m_testCtx, "vertex", "Vertex-only tests");
        addChild(vertexGroup);

        tcu::TestCaseGroup *fragmentGroup = new tcu::TestCaseGroup(m_testCtx, "fragment", "Fragment-only tests");
        addChild(fragmentGroup);

        for (int seed = 0; seed < 50; seed++)
        {
            vertexGroup->addChild(
                createRandomShaderCase(m_context, "Vector swizzles in vertex shader", params, seed, true, false));
            fragmentGroup->addChild(
                createRandomShaderCase(m_context, "Vector swizzles in fragment shader", params, seed, false, true));
        }
    }
};

class ComparisonOpsGroup : public TestCaseGroup
{
public:
    ComparisonOpsGroup(Context &context) : TestCaseGroup(context, "comparison_ops", "Comparison operators")
    {
    }

    void init(void)
    {
        rsg::ProgramParameters params;
        params.useScalarConversions = true;
        params.useComparisonOps     = true;

        tcu::TestCaseGroup *vertexGroup = new tcu::TestCaseGroup(m_testCtx, "vertex", "Vertex-only tests");
        addChild(vertexGroup);

        tcu::TestCaseGroup *fragmentGroup = new tcu::TestCaseGroup(m_testCtx, "fragment", "Fragment-only tests");
        addChild(fragmentGroup);

        for (int seed = 0; seed < 50; seed++)
        {
            vertexGroup->addChild(
                createRandomShaderCase(m_context, "Comparison operators in vertex shader", params, seed, true, false));
            fragmentGroup->addChild(createRandomShaderCase(m_context, "Comparison operators in fragment shader", params,
                                                           seed, false, true));
        }
    }
};

class ConditionalsGroup : public TestCaseGroup
{
public:
    ConditionalsGroup(Context &context) : TestCaseGroup(context, "conditionals", "Conditional control flow (if-else)")
    {
    }

    void init(void)
    {
        rsg::ProgramParameters params;
        params.useScalarConversions                     = true;
        params.useSwizzle                               = true;
        params.useComparisonOps                         = true;
        params.useConditionals                          = true;
        params.vertexParameters.maxStatementDepth       = 4;
        params.vertexParameters.maxStatementsPerBlock   = 5;
        params.fragmentParameters.maxStatementDepth     = 4;
        params.fragmentParameters.maxStatementsPerBlock = 5;

        tcu::TestCaseGroup *vertexGroup = new tcu::TestCaseGroup(m_testCtx, "vertex", "Vertex-only tests");
        addChild(vertexGroup);

        tcu::TestCaseGroup *fragmentGroup = new tcu::TestCaseGroup(m_testCtx, "fragment", "Fragment-only tests");
        addChild(fragmentGroup);

        tcu::TestCaseGroup *combinedGroup = new tcu::TestCaseGroup(m_testCtx, "combined", "Combined tests");
        addChild(combinedGroup);

        for (int seed = 0; seed < 100; seed++)
        {
            vertexGroup->addChild(createRandomShaderCase(m_context, "Conditional control flow in vertex shader", params,
                                                         seed, true, false));
            fragmentGroup->addChild(createRandomShaderCase(m_context, "Conditional control flow in fragment shader",
                                                           params, seed, false, true));
            combinedGroup->addChild(createRandomShaderCase(
                m_context, "Conditional control flow in vertex and fragment shaders", params, seed, true, true));
        }
    }
};

class TrigonometricGroup : public TestCaseGroup
{
public:
    TrigonometricGroup(Context &context) : TestCaseGroup(context, "trigonometric", "Trigonometric built-in functions")
    {
    }

    void init(void)
    {
        rsg::ProgramParameters params;
        params.useScalarConversions    = true;
        params.useSwizzle              = true;
        params.trigonometricBaseWeight = 4.0f;

        tcu::TestCaseGroup *vertexGroup = new tcu::TestCaseGroup(m_testCtx, "vertex", "Vertex-only tests");
        addChild(vertexGroup);

        tcu::TestCaseGroup *fragmentGroup = new tcu::TestCaseGroup(m_testCtx, "fragment", "Fragment-only tests");
        addChild(fragmentGroup);

        for (int seed = 0; seed < 100; seed++)
        {
            vertexGroup->addChild(
                createRandomShaderCase(m_context, "Trigonometric ops in vertex shader", params, seed, true, false));
            fragmentGroup->addChild(
                createRandomShaderCase(m_context, "Trigonometric ops in fragment shader", params, seed, false, true));
        }
    }
};

class ExponentialGroup : public TestCaseGroup
{
public:
    ExponentialGroup(Context &context) : TestCaseGroup(context, "exponential", "Exponential built-in functions")
    {
    }

    void init(void)
    {
        rsg::ProgramParameters params;
        params.useScalarConversions  = true;
        params.useSwizzle            = true;
        params.exponentialBaseWeight = 4.0f;

        tcu::TestCaseGroup *vertexGroup = new tcu::TestCaseGroup(m_testCtx, "vertex", "Vertex-only tests");
        addChild(vertexGroup);

        tcu::TestCaseGroup *fragmentGroup = new tcu::TestCaseGroup(m_testCtx, "fragment", "Fragment-only tests");
        addChild(fragmentGroup);

        for (int seed = 0; seed < 100; seed++)
        {
            vertexGroup->addChild(
                createRandomShaderCase(m_context, "Exponential ops in vertex shader", params, seed, true, false));
            fragmentGroup->addChild(
                createRandomShaderCase(m_context, "Exponential ops in fragment shader", params, seed, false, true));
        }
    }
};

class TextureGroup : public TestCaseGroup
{
public:
    TextureGroup(Context &context) : TestCaseGroup(context, "texture", "Texture lookups")
    {
    }

    void init(void)
    {
        rsg::ProgramParameters params;
        params.useScalarConversions                   = true;
        params.useSwizzle                             = true;
        params.vertexParameters.texLookupBaseWeight   = 10.0f;
        params.vertexParameters.useTexture2D          = true;
        params.vertexParameters.useTextureCube        = true;
        params.fragmentParameters.texLookupBaseWeight = 10.0f;
        params.fragmentParameters.useTexture2D        = true;
        params.fragmentParameters.useTextureCube      = true;

        tcu::TestCaseGroup *vertexGroup = new tcu::TestCaseGroup(m_testCtx, "vertex", "Vertex-only tests");
        addChild(vertexGroup);

        tcu::TestCaseGroup *fragmentGroup = new tcu::TestCaseGroup(m_testCtx, "fragment", "Fragment-only tests");
        addChild(fragmentGroup);

        // Do only 50 vertex cases and 150 fragment cases.
        for (int seed = 0; seed < 50; seed++)
            vertexGroup->addChild(
                createRandomShaderCase(m_context, "Texture lookups in vertex shader", params, seed, true, false));

        for (int seed = 0; seed < 150; seed++)
            fragmentGroup->addChild(
                createRandomShaderCase(m_context, "Texture lookups in fragment shader", params, seed, false, true));
    }
};

class AllFeaturesGroup : public TestCaseGroup
{
public:
    AllFeaturesGroup(Context &context) : TestCaseGroup(context, "all_features", "All features enabled")
    {
    }

    void init(void)
    {
        rsg::ProgramParameters params;
        params.useScalarConversions    = true;
        params.useSwizzle              = true;
        params.useComparisonOps        = true;
        params.useConditionals         = true;
        params.trigonometricBaseWeight = 1.0f;
        params.exponentialBaseWeight   = 1.0f;

        params.vertexParameters.maxStatementDepth            = 4;
        params.vertexParameters.maxStatementsPerBlock        = 7;
        params.vertexParameters.maxExpressionDepth           = 7;
        params.vertexParameters.maxCombinedVariableScalars   = 64;
        params.fragmentParameters.maxStatementDepth          = 4;
        params.fragmentParameters.maxStatementsPerBlock      = 7;
        params.fragmentParameters.maxExpressionDepth         = 7;
        params.fragmentParameters.maxCombinedVariableScalars = 64;

        params.fragmentParameters.texLookupBaseWeight =
            4.0f; // \note Texture lookups are enabled for fragment shaders only.
        params.fragmentParameters.useTexture2D   = true;
        params.fragmentParameters.useTextureCube = true;

        tcu::TestCaseGroup *vertexGroup = new tcu::TestCaseGroup(m_testCtx, "vertex", "Vertex-only tests");
        addChild(vertexGroup);

        tcu::TestCaseGroup *fragmentGroup = new tcu::TestCaseGroup(m_testCtx, "fragment", "Fragment-only tests");
        addChild(fragmentGroup);

        for (int seed = 0; seed < 100; seed++)
        {
            vertexGroup->addChild(
                createRandomShaderCase(m_context, "Texture lookups in vertex shader", params, seed, true, false));
            fragmentGroup->addChild(
                createRandomShaderCase(m_context, "Texture lookups in fragment shader", params, seed, false, true));
        }
    }
};

} // namespace

RandomShaderTests::RandomShaderTests(Context &context) : TestCaseGroup(context, "random", "Random shaders")
{
}

RandomShaderTests::~RandomShaderTests(void)
{
}

namespace
{

} // namespace

void RandomShaderTests::init(void)
{
    addChild(new BasicExpressionGroup(m_context));
    addChild(new ScalarConversionGroup(m_context));
    addChild(new SwizzleGroup(m_context));
    addChild(new ComparisonOpsGroup(m_context));
    addChild(new ConditionalsGroup(m_context));
    addChild(new TrigonometricGroup(m_context));
    addChild(new ExponentialGroup(m_context));
    addChild(new TextureGroup(m_context));
    addChild(new AllFeaturesGroup(m_context));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
