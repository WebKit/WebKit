#ifndef _GLSSHADERCONSTEXPRTESTS_HPP
#define _GLSSHADERCONSTEXPRTESTS_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL Shared Module
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
 * \brief Shared shader constant expression test components
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "gluShaderUtil.hpp"

#include <vector>

namespace tcu
{
class TestNode;
class TestContext;
} // namespace tcu

namespace glu
{
class RenderContext;
}

namespace deqp
{
namespace gls
{
namespace ShaderConstExpr
{
using glu::DataType;

struct TestParams
{
    const char *name;
    const char *expression;

    DataType inType;
    int minComponents;
    int maxComponents;

    DataType outType;
    float output;
};

enum TestShaderStage
{
    SHADER_VERTEX   = 1 << 0,
    SHADER_FRAGMENT = 1 << 2,
    SHADER_BOTH     = SHADER_VERTEX | SHADER_FRAGMENT,
};

std::vector<tcu::TestNode *> createTests(tcu::TestContext &testContext, glu::RenderContext &renderContext,
                                         const glu::ContextInfo &contextInfo, const TestParams *cases, int numCases,
                                         glu::GLSLVersion version, TestShaderStage testStage = SHADER_BOTH);

} // namespace ShaderConstExpr
} // namespace gls
} // namespace deqp

#endif // _GLSSHADERCONSTEXPRTESTS_HPP
