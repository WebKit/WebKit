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
 * \brief Performance tests.
 *//*--------------------------------------------------------------------*/

#include "es2pPerformanceTests.hpp"

#include "es2pBlendTests.hpp"
#include "es2pTextureFormatTests.hpp"
#include "es2pTextureFilteringTests.hpp"
#include "es2pTextureCountTests.hpp"
#include "es2pShaderOperatorTests.hpp"
#include "es2pShaderControlStatementTests.hpp"
#include "es2pShaderCompilerTests.hpp"
#include "es2pTextureUploadTests.hpp"
#include "es2pStateChangeCallTests.hpp"
#include "es2pStateChangeTests.hpp"
#include "es2pRedundantStateChangeTests.hpp"
#include "es2pDrawCallBatchingTests.hpp"
#include "es2pShaderOptimizationTests.hpp"

namespace deqp
{
namespace gles2
{
namespace Performance
{

// TextureTestGroup

class TextureTestGroup : public TestCaseGroup
{
public:
    TextureTestGroup(Context &context) : TestCaseGroup(context, "texture", "Texture Performance Tests")
    {
    }

    virtual void init(void)
    {
        addChild(new TextureFormatTests(m_context));
        addChild(new TextureFilteringTests(m_context));
        addChild(new TextureCountTests(m_context));
        addChild(new TextureUploadTests(m_context));
    }
};

// ShadersTestGroup

class ShadersTestGroup : public TestCaseGroup
{
public:
    ShadersTestGroup(Context &context) : TestCaseGroup(context, "shader", "Shader Performance Tests")
    {
    }

    virtual void init(void)
    {
        addChild(new ShaderOperatorTests(m_context));
        addChild(new ShaderControlStatementTests(m_context));
    }
};

// APITestGroup

class APITestGroup : public TestCaseGroup
{
public:
    APITestGroup(Context &context) : TestCaseGroup(context, "api", "API Performance Tests")
    {
    }

    virtual void init(void)
    {
        addChild(new StateChangeCallTests(m_context));
        addChild(new StateChangeTests(m_context));
        addChild(new RedundantStateChangeTests(m_context));
        addChild(new DrawCallBatchingTests(m_context));
    }
};

// PerformanceTests

PerformanceTests::PerformanceTests(Context &context) : TestCaseGroup(context, "performance", "Performance Tests")
{
}

PerformanceTests::~PerformanceTests(void)
{
}

void PerformanceTests::init(void)
{
    addChild(new BlendTests(m_context));
    addChild(new TextureTestGroup(m_context));
    addChild(new ShadersTestGroup(m_context));
    addChild(new ShaderCompilerTests(m_context));
    addChild(new APITestGroup(m_context));
}

} // namespace Performance
} // namespace gles2
} // namespace deqp
