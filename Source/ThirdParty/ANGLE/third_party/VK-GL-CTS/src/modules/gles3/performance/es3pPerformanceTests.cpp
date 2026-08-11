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
 * \brief Performance tests.
 *//*--------------------------------------------------------------------*/

#include "es3pPerformanceTests.hpp"

#include "es3pBlendTests.hpp"
#include "es3pTextureFormatTests.hpp"
#include "es3pTextureFilteringTests.hpp"
#include "es3pTextureCountTests.hpp"
#include "es3pShaderOperatorTests.hpp"
#include "es3pShaderControlStatementTests.hpp"
#include "es3pShaderCompilerTests.hpp"
#include "es3pShaderOptimizationTests.hpp"
#include "es3pRedundantStateChangeTests.hpp"
#include "es3pStateChangeCallTests.hpp"
#include "es3pStateChangeTests.hpp"
#include "es3pBufferDataUploadTests.hpp"
#include "es3pDepthTests.hpp"

namespace deqp
{
namespace gles3
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

// APITests

class APITests : public TestCaseGroup
{
public:
    APITests(Context &context) : TestCaseGroup(context, "api", "API Performance Tests")
    {
    }

    virtual void init(void)
    {
        addChild(new StateChangeCallTests(m_context));
        addChild(new StateChangeTests(m_context));
        addChild(new RedundantStateChangeTests(m_context));
    }
};

// BufferTestGroup

class BufferTestGroup : public TestCaseGroup
{
public:
    BufferTestGroup(Context &context) : TestCaseGroup(context, "buffer", "Buffer Performance Tests")
    {
    }

    virtual void init(void)
    {
        addChild(new BufferDataUploadTests(m_context));
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
    addChild(new APITests(m_context));
    addChild(new BufferTestGroup(m_context));
    addChild(new DepthTests(m_context));
}

} // namespace Performance
} // namespace gles3
} // namespace deqp
