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
 * \brief Shader execute test.
 *//*--------------------------------------------------------------------*/

#include "es2fShaderExecuteTest.hpp"
#include "glsShaderLibrary.hpp"

#include "deMemory.h"

#include <stdio.h>
#include <vector>
#include <string>

using namespace std;
using namespace tcu;
using namespace deqp::gls;

namespace deqp
{
namespace gles2
{
namespace Functional
{

ShaderExecuteTest::ShaderExecuteTest(Context &context, const char *groupName, const char *description)
    : TestCaseGroup(context, groupName, description)
{
}

ShaderExecuteTest::~ShaderExecuteTest(void)
{
}

void ShaderExecuteTest::init(void)
{
    // Test code.
    gls::ShaderLibrary shaderLibrary(m_testCtx, m_context.getRenderContext(), m_context.getContextInfo());
    string fileName             = string("shaders/") + getName() + ".test";
    vector<TestNode *> children = shaderLibrary.loadShaderFile(fileName.c_str());

    for (int i = 0; i < (int)children.size(); i++)
        addChild(children[i]);
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
