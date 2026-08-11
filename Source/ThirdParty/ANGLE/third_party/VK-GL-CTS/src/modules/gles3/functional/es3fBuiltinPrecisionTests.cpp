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
 * \brief Tests for precision and range of GLSL builtins and types.
 *//*--------------------------------------------------------------------*/

#include "es3fBuiltinPrecisionTests.hpp"

#include "deUniquePtr.hpp"
#include "glsBuiltinPrecisionTests.hpp"

#include <vector>

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace bpt = gls::BuiltinPrecisionTests;

TestCaseGroup *createBuiltinPrecisionTests(Context &context)
{
    TestCaseGroup *group = new TestCaseGroup(context, "precision", "Builtin precision tests");
    std::vector<glu::ShaderType> shaderTypes;
    de::MovePtr<const bpt::CaseFactories> es3Cases = bpt::createES3BuiltinCases();

    shaderTypes.push_back(glu::SHADERTYPE_VERTEX);
    shaderTypes.push_back(glu::SHADERTYPE_FRAGMENT);

    bpt::addBuiltinPrecisionTests(context.getTestContext(), context.getRenderContext(), *es3Cases, shaderTypes, *group);
    return group;
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
