#ifndef _GLSBUILTINPRECISIONTESTS_HPP
#define _GLSBUILTINPRECISIONTESTS_HPP

/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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

#include "deUniquePtr.hpp"
#include "tcuTestCase.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderUtil.hpp"

#include <vector>

namespace deqp
{
namespace gls
{
namespace BuiltinPrecisionTests
{

class CaseFactory;

class CaseFactories
{
public:
    virtual ~CaseFactories(void)
    {
    }
    virtual const std::vector<const CaseFactory *> getFactories(void) const = 0;
};

de::MovePtr<const CaseFactories> createES3BuiltinCases(void);
de::MovePtr<const CaseFactories> createES31BuiltinCases(void);

void addBuiltinPrecisionTests(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const CaseFactories &cases,
                              const std::vector<glu::ShaderType> &shaderTypes, tcu::TestCaseGroup &dstGroup);

} // namespace BuiltinPrecisionTests

using BuiltinPrecisionTests::addBuiltinPrecisionTests;

} // namespace gls
} // namespace deqp

#endif // _GLSBUILTINPRECISIONTESTS_HPP
