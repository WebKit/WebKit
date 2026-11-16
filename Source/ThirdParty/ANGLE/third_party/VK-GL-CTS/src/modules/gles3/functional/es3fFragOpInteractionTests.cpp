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
 * \brief Shader - render state interaction tests.
 *//*--------------------------------------------------------------------*/

#include "es3fFragOpInteractionTests.hpp"
#include "glsFragOpInteractionCase.hpp"
#include "deStringUtil.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

using gls::FragOpInteractionCase;

FragOpInteractionTests::FragOpInteractionTests(Context &context)
    : TestCaseGroup(context, "interaction", "Shader - Render State Interaction Tests")
{
}

FragOpInteractionTests::~FragOpInteractionTests(void)
{
}

void FragOpInteractionTests::init(void)
{
    // .basic
    {
        tcu::TestCaseGroup *const basicGroup = new tcu::TestCaseGroup(m_testCtx, "basic_shader", "Basic shaders");
        const uint32_t baseSeed              = 0xac2301cf;
        const int numCases                   = 100;
        rsg::ProgramParameters params;

        addChild(basicGroup);

        params.version = rsg::VERSION_300;

        params.useScalarConversions = true;
        params.useSwizzle           = true;
        params.useComparisonOps     = true;
        params.useConditionals      = true;

        params.vertexParameters.randomize                  = true;
        params.vertexParameters.maxStatementDepth          = 3;
        params.vertexParameters.maxStatementsPerBlock      = 4;
        params.vertexParameters.maxExpressionDepth         = 4;
        params.vertexParameters.maxCombinedVariableScalars = 64;

        params.fragmentParameters.randomize                  = true;
        params.fragmentParameters.maxStatementDepth          = 3;
        params.fragmentParameters.maxStatementsPerBlock      = 4;
        params.fragmentParameters.maxExpressionDepth         = 4;
        params.fragmentParameters.maxCombinedVariableScalars = 64;

        for (int ndx = 0; ndx < numCases; ndx++)
        {
            params.seed = baseSeed ^ deInt32Hash(ndx);
            basicGroup->addChild(new FragOpInteractionCase(m_testCtx, m_context.getRenderContext(),
                                                           m_context.getContextInfo(), de::toString(ndx).c_str(),
                                                           params));
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
