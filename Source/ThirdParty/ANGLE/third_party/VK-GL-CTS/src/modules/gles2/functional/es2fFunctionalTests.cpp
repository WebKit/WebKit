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
 * \brief Base class for a test case.
 *//*--------------------------------------------------------------------*/

#include "es2fFunctionalTests.hpp"

#include "es2fColorClearTest.hpp"
#include "es2fLightAmountTest.hpp"
#include "es2fMultisampledRenderToTextureTests.hpp"
#include "es2fShaderExecuteTest.hpp"
#include "es2fFboApiTest.hpp"
#include "es2fFboRenderTest.hpp"
#include "es2fFboCompletenessTests.hpp"
#include "es2fRandomShaderTests.hpp"
#include "es2fPrerequisiteTests.hpp"
#include "es2fDepthTests.hpp"
#include "es2fStencilTests.hpp"
#include "es2fScissorTests.hpp"
#include "es2fVertexArrayTest.hpp"
#include "es2fRasterizationTests.hpp"
#include "es2fDepthStencilClearTests.hpp"
#include "es2fDepthStencilTests.hpp"
#include "es2fBlendTests.hpp"
#include "es2fRandomFragmentOpTests.hpp"
#include "es2fMultisampleTests.hpp"
#include "es2fUniformApiTests.hpp"
#include "es2fBufferWriteTests.hpp"
#include "es2fImplementationLimitTests.hpp"
#include "es2fDepthRangeTests.hpp"
#include "es2fDitheringTests.hpp"
#include "es2fClippingTests.hpp"
#include "es2fClipControlTests.hpp"
#include "es2fPolygonOffsetTests.hpp"
#include "es2fDrawTests.hpp"
#include "es2fFragOpInteractionTests.hpp"
#include "es2fFlushFinishTests.hpp"
#include "es2fDefaultVertexAttributeTests.hpp"
#include "es2fLifetimeTests.hpp"

#include "es2fTextureFormatTests.hpp"
#include "es2fTextureWrapTests.hpp"
#include "es2fTextureFilteringTests.hpp"
#include "es2fTextureMipmapTests.hpp"
#include "es2fTextureSizeTests.hpp"
#include "es2fTextureSpecificationTests.hpp"
#include "es2fTextureCompletenessTests.hpp"
#include "es2fNegativeVertexArrayApiTests.hpp"
#include "es2fNegativeTextureApiTests.hpp"
#include "es2fNegativeFragmentApiTests.hpp"
#include "es2fNegativeBufferApiTests.hpp"
#include "es2fNegativeShaderApiTests.hpp"
#include "es2fNegativeStateApiTests.hpp"
#include "es2fVertexTextureTests.hpp"
#include "es2fTextureUnitTests.hpp"

#include "es2fShaderApiTests.hpp"
#include "es2fShaderAlgorithmTests.hpp"
#include "es2fShaderBuiltinVarTests.hpp"
#include "es2fShaderConstExprTests.hpp"
#include "es2fShaderDiscardTests.hpp"
#include "es2fShaderFunctionTests.hpp"
#include "es2fShaderIndexingTests.hpp"
#include "es2fShaderLoopTests.hpp"
#include "es2fShaderOperatorTests.hpp"
#include "es2fShaderReturnTests.hpp"
#include "es2fShaderStructTests.hpp"
#include "es2fShaderMatrixTests.hpp"
#include "es2fShaderTextureFunctionTests.hpp"
#include "es2fAttribLocationTests.hpp"
#include "es2fShaderInvarianceTests.hpp"
#include "es2fShaderFragDataTests.hpp"

// State query tests
#include "es2fBooleanStateQueryTests.hpp"
#include "es2fIntegerStateQueryTests.hpp"
#include "es2fFloatStateQueryTests.hpp"
#include "es2fTextureStateQueryTests.hpp"
#include "es2fStringQueryTests.hpp"
#include "es2fBufferObjectQueryTests.hpp"
#include "es2fFboStateQueryTests.hpp"
#include "es2fRboStateQueryTests.hpp"
#include "es2fShaderStateQueryTests.hpp"

#include "es2fReadPixelsTests.hpp"
#include "es2fDebugMarkerTests.hpp"

namespace deqp
{
namespace gles2
{
namespace Functional
{

// ShadersTestGroup

class ShadersTestGroup : public TestCaseGroup
{
public:
    ShadersTestGroup(Context &context) : TestCaseGroup(context, "shaders", "Shader Tests")
    {
    }

    virtual ~ShadersTestGroup(void)
    {
    }

    virtual void init(void)
    {
        addChild(new ShaderExecuteTest(m_context, "preprocessor", "Preprocessor Tests"));
        addChild(new ShaderExecuteTest(m_context, "constants", "Constant Literal Tests"));
        addChild(new ShaderExecuteTest(m_context, "linkage", "Linkage Tests"));
        addChild(new ShaderExecuteTest(m_context, "conversions", "Type Conversion Tests"));
        addChild(new ShaderExecuteTest(m_context, "conditionals", "Conditionals Tests"));
        addChild(new ShaderExecuteTest(m_context, "declarations", "Declarations Tests"));
        addChild(new ShaderExecuteTest(m_context, "swizzles", "Swizzle Tests"));
        addChild(new ShaderExecuteTest(m_context, "functions", "Function Tests"));
        addChild(new ShaderExecuteTest(m_context, "keywords", "Keyword Tests"));
        addChild(new ShaderExecuteTest(m_context, "reserved_operators", "Reserved Operator Tests"));
        addChild(new ShaderExecuteTest(m_context, "qualification_order", "Order of Qualification Tests"));
        addChild(new ShaderExecuteTest(m_context, "scoping", "Scoping of Declarations"));
        addChild(new ShaderExecuteTest(m_context, "invalid_implicit_conversions", "Invalid Implicit Conversions"));
        addChild(new ShaderExecuteTest(m_context, "misc", "Miscellaneous Tests"));

        addChild(new ShaderIndexingTests(m_context));
        addChild(new ShaderLoopTests(m_context));
        addChild(new ShaderOperatorTests(m_context));
        addChild(new ShaderMatrixTests(m_context));
        addChild(new ShaderReturnTests(m_context));
        addChild(new ShaderDiscardTests(m_context));
        addChild(new ShaderStructTests(m_context));
        addChild(new ShaderBuiltinVarTests(m_context));
        addChild(new ShaderTextureFunctionTests(m_context));
        addChild(new ShaderInvarianceTests(m_context));
        addChild(new ShaderFragDataTests(m_context));
        addChild(new ShaderAlgorithmTests(m_context));
        addChild(new ShaderConstExprTests(m_context));
        addChild(new ShaderFunctionTests(m_context));

        addChild(new RandomShaderTests(m_context));
    }
};

// TextureTestGroup

class TextureTestGroup : public TestCaseGroup
{
public:
    TextureTestGroup(Context &context) : TestCaseGroup(context, "texture", "Texture Tests")
    {
    }

    virtual ~TextureTestGroup(void)
    {
    }

    virtual void init(void)
    {
        addChild(new TextureFormatTests(m_context));
        addChild(new TextureSizeTests(m_context));
        addChild(new TextureWrapTests(m_context));
        addChild(new TextureFilteringTests(m_context));
        addChild(new TextureMipmapTests(m_context));
        addChild(new TextureSpecificationTests(m_context));
        addChild(new TextureCompletenessTests(m_context));
        addChild(new VertexTextureTests(m_context));
        addChild(new TextureUnitTests(m_context));
    }

    virtual void deinit(void)
    {
    }
};

class BufferTests : public TestCaseGroup
{
public:
    BufferTests(Context &context) : TestCaseGroup(context, "buffer", "Buffer object tests")
    {
    }

    void init(void)
    {
        addChild(new BufferWriteTests(m_context));
    }
};

// FboTestGroup

class FboTestGroup : public TestCaseGroup
{
public:
    FboTestGroup(Context &context) : TestCaseGroup(context, "fbo", "Framebuffer Object Tests")
    {
    }

    virtual ~FboTestGroup(void)
    {
    }

    virtual void init(void)
    {
        addChild(new FboApiTestGroup(m_context));
        addChild(new FboRenderTestGroup(m_context));
        addChild(createFboCompletenessTests(m_context));
    }
};

// NegativeApiTestGroup

class NegativeApiTestGroup : public TestCaseGroup
{
public:
    NegativeApiTestGroup(Context &context) : TestCaseGroup(context, "negative_api", "Negative API Tests")
    {
    }

    virtual ~NegativeApiTestGroup(void)
    {
    }

    virtual void init(void)
    {
        addChild(new NegativeBufferApiTests(m_context));
        addChild(new NegativeFragmentApiTests(m_context));
        addChild(new NegativeShaderApiTests(m_context));
        addChild(new NegativeStateApiTests(m_context));
        addChild(new NegativeTextureApiTests(m_context));
        addChild(new NegativeVertexArrayApiTests(m_context));
    }
};

// FragmentOpTests

class FragmentOpTests : public TestCaseGroup
{
public:
    FragmentOpTests(Context &context) : TestCaseGroup(context, "fragment_ops", "Per-Fragment Operation Tests")
    {
    }

    void init(void)
    {
        addChild(new DepthTests(m_context));
        addChild(new StencilTests(m_context));
        addChild(new DepthStencilTests(m_context));
        addChild(new ScissorTests(m_context));
        addChild(new BlendTests(m_context));
        addChild(new RandomFragmentOpTests(m_context));
        addChild(new FragOpInteractionTests(m_context));
    }
};

// StateQueryTests

class StateQueryTests : public TestCaseGroup
{
public:
    StateQueryTests(Context &context) : TestCaseGroup(context, "state_query", "State Query Tests")
    {
    }

    void init(void)
    {
        addChild(new BooleanStateQueryTests(m_context));
        addChild(new IntegerStateQueryTests(m_context));
        addChild(new FloatStateQueryTests(m_context));
        addChild(new TextureStateQueryTests(m_context));
        addChild(new StringQueryTests(m_context));
        addChild(new BufferObjectQueryTests(m_context));
        addChild(new FboStateQueryTests(m_context));
        addChild(new RboStateQueryTests(m_context));
        addChild(new ShaderStateQueryTests(m_context));
    }
};

// FunctionalTestGroup

FunctionalTests::FunctionalTests(Context &context) : TestCaseGroup(context, "functional", "Functionality Tests")
{
}

FunctionalTests::~FunctionalTests(void)
{
}

void FunctionalTests::init(void)
{
    addChild(new PrerequisiteTests(m_context));
    addChild(new ImplementationLimitTests(m_context));
    addChild(new ClipControlTests(m_context));
    addChild(new ColorClearTest(m_context));
    addChild(new DepthStencilClearTests(m_context));
    addChild(new BufferTests(m_context));
    addChild(new LightAmountTest(m_context));
    addChild(new MultisampledRenderToTextureTests(m_context));
    addChild(new ShadersTestGroup(m_context));
    addChild(new TextureTestGroup(m_context));
    addChild(new FragmentOpTests(m_context));
    addChild(new FboTestGroup(m_context));
    addChild(new VertexArrayTestGroup(m_context));
    addChild(new ShaderApiTests(m_context));
    addChild(new NegativeApiTestGroup(m_context));
    addChild(new RasterizationTests(m_context));
    addChild(createAttributeLocationTests(m_context));
    addChild(new MultisampleTests(m_context));
    addChild(new UniformApiTests(m_context));
    addChild(new ReadPixelsTests(m_context));
    addChild(new DepthRangeTests(m_context));
    addChild(new DitheringTests(m_context));
    addChild(new StateQueryTests(m_context));
    addChild(new ClippingTests(m_context));
    addChild(new PolygonOffsetTests(m_context));
    addChild(new DrawTests(m_context));
    addChild(new FlushFinishTests(m_context));
    addChild(new DefaultVertexAttributeTests(m_context));
    addChild(createLifetimeTests(m_context));
    addChild(createDebugMarkerTests(m_context));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
