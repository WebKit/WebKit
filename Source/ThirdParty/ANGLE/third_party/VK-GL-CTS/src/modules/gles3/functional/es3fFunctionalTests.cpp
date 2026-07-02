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
 * \brief Functional Test Group.
 *//*--------------------------------------------------------------------*/

#include "es3fFunctionalTests.hpp"

#include "es3fColorClearTest.hpp"
#include "es3fDepthTests.hpp"
#include "es3fPrerequisiteTests.hpp"
#include "es3fStencilTests.hpp"
#include "es3fDepthStencilTests.hpp"
#include "es3fVertexArrayTest.hpp"
#include "es3fUniformBlockTests.hpp"
#include "es3fUniformApiTests.hpp"
#include "es3fFragmentOutputTests.hpp"
#include "es3fOcclusionQueryTests.hpp"
#include "es3fDepthStencilClearTests.hpp"
#include "es3fSamplerObjectTests.hpp"
#include "es3fAttribLocationTests.hpp"
#include "es3fPixelBufferObjectTests.hpp"
#include "es3fRasterizationTests.hpp"
#include "es3fRasterizerDiscardTests.hpp"
#include "es3fTransformFeedbackTests.hpp"
#include "es3fVertexArrayObjectTests.hpp"
#include "es3fPrimitiveRestartTests.hpp"
#include "es3fInstancedRenderingTests.hpp"
#include "es3fSyncTests.hpp"
#include "es3fBlendTests.hpp"
#include "es3fRandomFragmentOpTests.hpp"
#include "es3fMultisampleTests.hpp"
#include "es3fMultiviewTests.hpp"
#include "es3fImplementationLimitTests.hpp"
#include "es3fDitheringTests.hpp"
#include "es3fClippingTests.hpp"
#include "es3fPolygonOffsetTests.hpp"
#include "es3fDrawTests.hpp"
#include "es3fFragOpInteractionTests.hpp"
#include "es3fFlushFinishTests.hpp"
#include "es3fFlushFinishTests.hpp"
#include "es3fDefaultVertexAttributeTests.hpp"
#include "es3fScissorTests.hpp"
#include "es3fLifetimeTests.hpp"
#include "es3fDefaultVertexArrayObjectTests.hpp"
#include "es3fDrawBuffersIndexedTests.hpp"

// Shader tests
#include "es3fShaderApiTests.hpp"
#include "es3fShaderConstExprTests.hpp"
#include "es3fShaderDiscardTests.hpp"
#include "es3fShaderFunctionTests.hpp"
#include "es3fShaderIndexingTests.hpp"
#include "es3fShaderLoopTests.hpp"
#include "es3fShaderMatrixTests.hpp"
#include "es3fShaderOperatorTests.hpp"
#include "es3fShaderReturnTests.hpp"
#include "es3fShaderStructTests.hpp"
#include "es3fShaderSwitchTests.hpp"
#include "es3fRandomShaderTests.hpp"
#include "es3fFragDepthTests.hpp"
#include "es3fShaderPrecisionTests.hpp"
#include "es3fShaderBuiltinVarTests.hpp"
#include "es3fShaderTextureFunctionTests.hpp"
#include "es3fShaderDerivateTests.hpp"
#include "es3fShaderPackingFunctionTests.hpp"
#include "es3fShaderCommonFunctionTests.hpp"
#include "es3fShaderInvarianceTests.hpp"
#include "es3fShaderFragDataTests.hpp"
#include "es3fBuiltinPrecisionTests.hpp"
#include "es3fShaderMetamorphicTests.hpp"

// Texture tests
#include "es3fTextureFormatTests.hpp"
#include "es3fTextureWrapTests.hpp"
#include "es3fTextureFilteringTests.hpp"
#include "es3fTextureMipmapTests.hpp"
#include "es3fTextureSizeTests.hpp"
#include "es3fTextureSwizzleTests.hpp"
#include "es3fTextureShadowTests.hpp"
#include "es3fTextureSpecificationTests.hpp"
#include "es3fVertexTextureTests.hpp"
#include "es3fTextureUnitTests.hpp"
#include "es3fCompressedTextureTests.hpp"

// Fbo tests
#include "es3fFboApiTests.hpp"
#include "es3fFboCompletenessTests.hpp"
#include "es3fFboColorbufferTests.hpp"
#include "es3fFboDepthbufferTests.hpp"
#include "es3fFboStencilbufferTests.hpp"
#include "es3fFramebufferBlitTests.hpp"
#include "es3fFboMultisampleTests.hpp"
#include "es3fFboRenderTest.hpp"
#include "es3fFboInvalidateTests.hpp"

// Buffer tests
#include "es3fBufferWriteTests.hpp"
#include "es3fBufferMapTests.hpp"
#include "es3fBufferCopyTests.hpp"

// Negative API tests
#include "es3fNegativeBufferApiTests.hpp"
#include "es3fNegativeTextureApiTests.hpp"
#include "es3fNegativeShaderApiTests.hpp"
#include "es3fNegativeFragmentApiTests.hpp"
#include "es3fNegativeVertexArrayApiTests.hpp"
#include "es3fNegativeStateApiTests.hpp"

// State query tests
#include "es3fBooleanStateQueryTests.hpp"
#include "es3fIntegerStateQueryTests.hpp"
#include "es3fInteger64StateQueryTests.hpp"
#include "es3fFloatStateQueryTests.hpp"
#include "es3fTextureStateQueryTests.hpp"
#include "es3fStringQueryTests.hpp"
#include "es3fSamplerStateQueryTests.hpp"
#include "es3fBufferObjectQueryTests.hpp"
#include "es3fFboStateQueryTests.hpp"
#include "es3fRboStateQueryTests.hpp"
#include "es3fShaderStateQueryTests.hpp"
#include "es3fInternalFormatQueryTests.hpp"
#include "es3fIndexedStateQueryTests.hpp"

#include "es3fReadPixelsTests.hpp"

#include "glsShaderLibrary.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

class ShaderLibraryTest : public TestCaseGroup
{
public:
    ShaderLibraryTest(Context &context, const char *name, const char *description)
        : TestCaseGroup(context, name, description)
    {
    }

    void init(void)
    {
        gls::ShaderLibrary shaderLibrary(m_testCtx, m_context.getRenderContext(), m_context.getContextInfo());
        std::string fileName                  = std::string("shaders/") + getName() + ".test";
        std::vector<tcu::TestNode *> children = shaderLibrary.loadShaderFile(fileName.c_str());

        for (int i = 0; i < (int)children.size(); i++)
            addChild(children[i]);
    }
};

class ShaderBuiltinFunctionTests : public TestCaseGroup
{
public:
    ShaderBuiltinFunctionTests(Context &context)
        : TestCaseGroup(context, "builtin_functions", "Built-in Function Tests")
    {
    }

    void init(void)
    {
        addChild(new ShaderCommonFunctionTests(m_context));
        addChild(new ShaderPackingFunctionTests(m_context));
        addChild(createBuiltinPrecisionTests(m_context));
    }
};

class ShaderTests : public TestCaseGroup
{
public:
    ShaderTests(Context &context) : TestCaseGroup(context, "shaders", "Shading Language Tests")
    {
    }

    void init(void)
    {
        addChild(new ShaderLibraryTest(m_context, "preprocessor", "Preprocessor Tests"));
        addChild(new ShaderLibraryTest(m_context, "constants", "Constant Literal Tests"));
        addChild(new ShaderLibraryTest(m_context, "linkage", "Linkage Tests"));
        addChild(new ShaderLibraryTest(m_context, "conversions", "Type Conversion Tests"));
        addChild(new ShaderLibraryTest(m_context, "conditionals", "Conditionals Tests"));
        addChild(new ShaderLibraryTest(m_context, "declarations", "Declarations Tests"));
        addChild(new ShaderLibraryTest(m_context, "swizzles", "Swizzle Tests"));
        addChild(new ShaderLibraryTest(m_context, "swizzle_math_operations", "Swizzle Math Operations Tests"));
        addChild(new ShaderLibraryTest(m_context, "functions", "Function Tests"));
        addChild(new ShaderLibraryTest(m_context, "arrays", "Array Tests"));
        addChild(new ShaderLibraryTest(m_context, "large_constant_arrays", "Large Constant Array Tests"));
        addChild(new ShaderLibraryTest(m_context, "keywords", "Keyword Tests"));
        addChild(new ShaderLibraryTest(m_context, "qualification_order", "Order Of Qualification Tests"));
        addChild(new ShaderLibraryTest(m_context, "scoping", "Scoping of Declarations"));
        addChild(new ShaderLibraryTest(m_context, "negative", "Miscellaneous Negative Shader Compilation Tests"));
        addChild(new ShaderLibraryTest(m_context, "uniform_block", "Uniform block tests"));
        addChild(new ShaderLibraryTest(m_context, "invalid_implicit_conversions", "Invalid Implicit Conversions"));

        addChild(new ShaderDiscardTests(m_context));
        addChild(new ShaderFunctionTests(m_context));
        addChild(new ShaderIndexingTests(m_context));
        addChild(new ShaderLoopTests(m_context));
        addChild(new ShaderOperatorTests(m_context));
        addChild(new ShaderMatrixTests(m_context));
        addChild(new ShaderReturnTests(m_context));
        addChild(new ShaderStructTests(m_context));
        addChild(new ShaderSwitchTests(m_context));
        addChild(new FragDepthTests(m_context));
        addChild(new ShaderPrecisionTests(m_context));
        addChild(new ShaderBuiltinVarTests(m_context));
        addChild(new ShaderTextureFunctionTests(m_context)); // \todo [pyry] Move to builtin?
        addChild(new ShaderDerivateTests(m_context));        // \todo [pyry] Move to builtin?
        addChild(new ShaderBuiltinFunctionTests(m_context));
        addChild(new ShaderInvarianceTests(m_context));
        addChild(new ShaderFragDataTests(m_context));
        addChild(new ShaderConstExprTests(m_context));
        addChild(new ShaderMetamorphicTests(m_context));
        addChild(new RandomShaderTests(m_context));
    }
};

class TextureTests : public TestCaseGroup
{
public:
    TextureTests(Context &context) : TestCaseGroup(context, "texture", "Texture Tests")
    {
    }

    void init(void)
    {
        addChild(new TextureFormatTests(m_context));
        addChild(new TextureSizeTests(m_context));
        addChild(new TextureWrapTests(m_context));
        addChild(new TextureFilteringTests(m_context));
        addChild(new TextureMipmapTests(m_context));
        addChild(new TextureSwizzleTests(m_context));
        addChild(new TextureShadowTests(m_context));
        addChild(new TextureSpecificationTests(m_context));
        addChild(new VertexTextureTests(m_context));
        addChild(new TextureUnitTests(m_context));
        addChild(new CompressedTextureTests(m_context));
    }
};

class FboTests : public TestCaseGroup
{
public:
    FboTests(Context &context) : TestCaseGroup(context, "fbo", "Framebuffer Object Tests")
    {
    }

    void init(void)
    {
        addChild(new FboApiTests(m_context));
        addChild(createFboCompletenessTests(m_context));
        addChild(new FboRenderTestGroup(m_context));
        addChild(new FboColorTests(m_context));
        addChild(new FboDepthTests(m_context));
        addChild(new FboStencilTests(m_context));
        addChild(new FramebufferBlitTests(m_context));
        addChild(new FboMultisampleTests(m_context));
        addChild(new MultiviewTests(m_context));
        addChild(new FboInvalidateTests(m_context));
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
        addChild(new BufferMapTests(m_context));
        addChild(new BufferCopyTests(m_context));
    }
};

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
        addChild(new NegativeTextureApiTests(m_context));
        addChild(new NegativeShaderApiTests(m_context));
        addChild(new NegativeFragmentApiTests(m_context));
        addChild(new NegativeVertexArrayApiTests(m_context));
        addChild(new NegativeStateApiTests(m_context));
    }
};

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
        addChild(new BlendTests(m_context));
        addChild(new RandomFragmentOpTests(m_context));
        addChild(new FragOpInteractionTests(m_context));
        addChild(new ScissorTests(m_context));
    }
};

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
        addChild(new Integer64StateQueryTests(m_context));
        addChild(new FloatStateQueryTests(m_context));
        addChild(new IndexedStateQueryTests(m_context));
        addChild(new TextureStateQueryTests(m_context));
        addChild(new StringQueryTests(m_context));
        addChild(new SamplerStateQueryTests(m_context));
        addChild(new BufferObjectQueryTests(m_context));
        addChild(new FboStateQueryTests(m_context));
        addChild(new RboStateQueryTests(m_context));
        addChild(new ShaderStateQueryTests(m_context));
        addChild(new InternalFormatQueryTests(m_context));
    }
};

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
    addChild(new ColorClearTest(m_context));
    addChild(new DepthStencilClearTests(m_context));
    addChild(new BufferTests(m_context));
    addChild(new ShaderTests(m_context));
    addChild(new TextureTests(m_context));
    addChild(new FragmentOpTests(m_context));
    addChild(new FboTests(m_context));
    addChild(new VertexArrayTestGroup(m_context));
    addChild(new UniformBlockTests(m_context));
    addChild(new UniformApiTests(m_context));
    addChild(createAttributeLocationTests(m_context));
    addChild(new FragmentOutputTests(m_context));
    addChild(new SamplerObjectTests(m_context));
    addChild(new PixelBufferObjectTests(m_context));
    addChild(new RasterizationTests(m_context));
    addChild(new OcclusionQueryTests(m_context));
    addChild(new VertexArrayObjectTestGroup(m_context));
    addChild(new PrimitiveRestartTests(m_context));
    addChild(new InstancedRenderingTests(m_context));
    addChild(new RasterizerDiscardTests(m_context));
    addChild(new TransformFeedbackTests(m_context));
    addChild(new SyncTests(m_context));
    addChild(new ShaderApiTests(m_context));
    addChild(new NegativeApiTestGroup(m_context));
    addChild(new MultisampleTests(m_context));
    addChild(new ReadPixelsTests(m_context));
    addChild(new DitheringTests(m_context));
    addChild(new StateQueryTests(m_context));
    addChild(new ClippingTests(m_context));
    addChild(new PolygonOffsetTests(m_context));
    addChild(new DrawTests(m_context));
    addChild(new FlushFinishTests(m_context));
    addChild(new DefaultVertexAttributeTests(m_context));
    addChild(createLifetimeTests(m_context));
    addChild(new DefaultVertexArrayObjectTests(m_context));
    addChild(createDrawBuffersIndexedTests(m_context));
}

GL45ES3FunctionalTests::GL45ES3FunctionalTests(Context &context)
    : TestCaseGroup(context, "functional", "Functionality Tests")
{
}

GL45ES3FunctionalTests::~GL45ES3FunctionalTests(void)
{
}

void GL45ES3FunctionalTests::init(void)
{
    addChild(createDrawBuffersIndexedTests(m_context));
    addChild(new StateQueryTests(m_context));
    addChild(new NegativeApiTestGroup(m_context));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
