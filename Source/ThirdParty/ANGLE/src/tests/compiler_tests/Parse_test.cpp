//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Parse_test.cpp:
//   Test for parsing erroneous and correct GLSL input.
//

#include <memory>
#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "compiler/translator/glsl/TranslatorESSL.h"
#include "gtest/gtest.h"

using namespace sh;

class ParseTest : public testing::Test
{
  public:
    ParseTest()
    {
        InitBuiltInResources(&mResources);
        mResources.FragmentPrecisionHigh = 1;
        mCompileOptions.intermediateTree = true;
    }

  protected:
    void TearDown() override { mTranslator.reset(); }

    testing::AssertionResult compile(const std::string &shaderString)
    {
        if (mTranslator == nullptr)
        {
            std::unique_ptr<TranslatorESSL> translator =
                std::make_unique<TranslatorESSL>(GL_FRAGMENT_SHADER, mShaderSpec);
            if (!translator->Init(mResources))
            {
                return testing::AssertionFailure() << "Failed to initialize translator";
            }
            mTranslator = std::move(translator);
        }

        const char *shaderStrings[] = {shaderString.c_str()};
        bool compilationSuccess     = mTranslator->compile(shaderStrings, 1, mCompileOptions);
        TInfoSink &infoSink         = mTranslator->getInfoSink();
        mInfoLog                    = RemoveSymbolIdsFromInfoLog(infoSink.info.c_str());
        if (!compilationSuccess)
        {
            return testing::AssertionFailure() << "Shader compilation failed " << mInfoLog;
        }
        return testing::AssertionSuccess();
    }

    bool foundErrorInIntermediateTree() const { return foundInIntermediateTree("ERROR:"); }

    bool foundInIntermediateTree(const char *stringToFind) const
    {
        return mInfoLog.find(stringToFind) != std::string::npos;
    }

    ShBuiltInResources mResources;
    ShCompileOptions mCompileOptions{};
    ShShaderSpec mShaderSpec = SH_WEBGL_SPEC;

  private:
    // Remove symbol ids from info log - the tests don't care about them.
    static std::string RemoveSymbolIdsFromInfoLog(const char *infoLog)
    {
        std::string filteredLog(infoLog);
        size_t idPrefixPos = 0u;
        do
        {
            idPrefixPos = filteredLog.find(" (symbol id");
            if (idPrefixPos != std::string::npos)
            {
                size_t idSuffixPos = filteredLog.find(")", idPrefixPos);
                filteredLog.erase(idPrefixPos, idSuffixPos - idPrefixPos + 1u);
            }
        } while (idPrefixPos != std::string::npos);
        return filteredLog;
    }

    std::unique_ptr<TranslatorESSL> mTranslator;
    std::string mInfoLog;
};

TEST_F(ParseTest, UnsizedArrayConstructorNoCrash)
{
    const char kShader[] = R"(#version 310 es\n"
int A[];
int B[int[][](A)];)";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("constructing from an unsized array"));
}

TEST_F(ParseTest, UniformBlockNameReferenceNoCrash)
{
    const char kShader[] = R"(#version 300 es
precision mediump float;
out float o;
uniform a { float r; } UBOA;
void main() {
    o = float(UBOA);
})";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree(
        "interface block cannot be used as a constructor argument for this type"));
}

TEST_F(ParseTest, Precise320NoCrash)
{
    const char kShader[] = R"(#version 320 es
precision mediump float;
void main(){
    float t;
    precise t;
})";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("unsupported shader version"));
}

// Tests that layout(index=0) is parsed in es 100 shaders if an extension like
// EXT_shader_framebuffer_fetch is enabled, but this does not cause a crash.
TEST_F(ParseTest, ShaderFramebufferFetchLayoutIndexNoCrash)
{
    mResources.EXT_blend_func_extended      = 1;
    mResources.MaxDualSourceDrawBuffers     = 1;
    mResources.EXT_shader_framebuffer_fetch = 1;
    const char kShader[]                    = R"(
#extension GL_EXT_blend_func_extended: require
#extension GL_EXT_shader_framebuffer_fetch : require
layout(index=0)mediump vec4 c;
void main() { }
)";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("'index' : invalid layout qualifier"));
}

TEST_F(ParseTest, Radians320NoCrash)
{
    const char kShader[] = R"(#version 320 es
precision mediump float;
vec4 s() { writeonly vec4 color; radians(color); return vec4(1); })";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("'writeonly' : Only allowed with shader storage blocks,"));
    EXPECT_TRUE(foundInIntermediateTree(
        "'radians' : wrong operand type - no operation 'radians' exists that"));
}

TEST_F(ParseTest, CoherentCoherentNoCrash)
{
    const char kShader[] = R"(#version 310 es
uniform highp coherent coherent readonly image2D image1;\n"
void main() {
})";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("coherent specified multiple times"));
}

TEST_F(ParseTest, LargeArrayIndexNoCrash)
{
    mShaderSpec          = SH_WEBGL2_SPEC;
    const char kShader[] = R"(#version 300 es
int rr[~1U];
out int o;
void main() {
    o = rr[1];
})";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(
        foundInIntermediateTree("Size of declared variable exceeds implementation-defined limit"));
}

// Tests that separating variable declaration of multiple instances of a anonymous structure
// rewrites the expression types for expressions that use the variables. At the time of writing
// the expression types were left referencing the original anonymous function.
TEST_F(ParseTest, SeparateAnonymousFunctionsRewritesExpressions)
{
    const char kShader[] = R"(
struct {
    mediump vec2 d;
} s0, s1;
void main() {
    s0 = s0;
    s1 = s1;
})";
    EXPECT_TRUE(compile(kShader));
    EXPECT_FALSE(foundInIntermediateTree("anonymous"));
}

// Tests that constant folding a division of a void variable does not crash during parsing.
TEST_F(ParseTest, ConstStructWithVoidAndDivNoCrash)
{
    const char kShader[] = R"(
const struct s { void i; } ss = s();
void main() {
    highp vec3 q = ss.i / ss.i;
})";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("illegal use of type 'void'"));
    EXPECT_TRUE(foundInIntermediateTree("constructor does not have any arguments"));
    EXPECT_TRUE(foundInIntermediateTree("operation with void operands"));
    EXPECT_TRUE(foundInIntermediateTree(
        "wrong operand types - no operation '/' exists that takes a left-hand operand of type "
        "'const void' and a right operand of type 'const void'"));
    EXPECT_TRUE(foundInIntermediateTree(
        "cannot convert from 'const void' to 'highp 3-component vector of float'"));
}

// Tests that division of void variable returns the same errors as division of constant
// void variable (see above).
TEST_F(ParseTest, StructWithVoidAndDivErrorCheck)
{
    const char kShader[] = R"(
struct s { void i; } ss = s();
void main() {
    highp vec3 q = ss.i / ss.i;
})";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("illegal use of type 'void'"));
    EXPECT_TRUE(foundInIntermediateTree("constructor does not have any arguments"));
    EXPECT_TRUE(foundInIntermediateTree("operation with void operands"));
    EXPECT_TRUE(foundInIntermediateTree(
        "wrong operand types - no operation '/' exists that takes a left-hand operand of type "
        "'void' and a right operand of type 'void'"));
    EXPECT_TRUE(foundInIntermediateTree(
        "cannot convert from 'void' to 'highp 3-component vector of float'"));
}

// Tests that usage of BuildIn struct type name does not crash during parsing.
TEST_F(ParseTest, BuildInStructTypeNameDeclarationNoCrash)
{
    mCompileOptions.validateAST = 1;
    const char kShader[]        = R"(
void main() {
gl_DepthRangeParameters testVariable;
})";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("reserved built-in name"));
}

TEST_F(ParseTest, BuildInStructTypeNameFunctionArgumentNoCrash)
{
    mCompileOptions.validateAST = 1;
    const char kShader[]        = R"(
void testFunction(gl_DepthRangeParameters testParam){}
void main() {
testFunction(gl_DepthRange);
})";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("reserved built-in name"));
}

TEST_F(ParseTest, BuildInStructTypeNameFunctionReturnValueNoCrash)
{
    mCompileOptions.validateAST = 1;
    const char kShader[]        = R"(
gl_DepthRangeParameters testFunction(){return gl_DepthRange;}
void main() {
testFunction();
})";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("reserved built-in name"));
}

// Tests that imod of const void variable does not crash during parsing.
TEST_F(ParseTest, ConstStructVoidAndImodAndNoCrash)
{
    const char kShader[] = R"(#version 310 es
const struct s { void i; } ss = s();
void main() {
    highp vec3 q = ss.i % ss.i;
})";
    EXPECT_FALSE(compile(kShader));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("illegal use of type 'void'"));
    EXPECT_TRUE(foundInIntermediateTree("constructor does not have any arguments"));
    EXPECT_TRUE(foundInIntermediateTree("operation with void operands"));
    EXPECT_TRUE(foundInIntermediateTree(
        "wrong operand types - no operation '%' exists that takes a left-hand operand of type "
        "'const void' and a right operand of type 'const void'"));
    EXPECT_TRUE(foundInIntermediateTree(
        "cannot convert from 'const void' to 'highp 3-component vector of float'"));
}

TEST_F(ParseTest, HugeUnsizedMultidimensionalArrayConstructorNoCrash)
{
    mCompileOptions.limitExpressionComplexity = true;
    std::ostringstream shader;
    shader << R"(#version 310 es
int E=int)";
    for (int i = 0; i < 10000; ++i)
    {
        shader << "[]";
    }
    shader << "()";
    EXPECT_FALSE(compile(shader.str()));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("array has too many dimensions"));
}

TEST_F(ParseTest, HugeMultidimensionalArrayConstructorNoCrash)
{
    mCompileOptions.limitExpressionComplexity = true;
    std::ostringstream shader;
    shader << R"(#version 310 es
int E=int)";
    for (int i = 0; i < 10000; ++i)
    {
        shader << "[1]";
    }

    for (int i = 0; i < 10000; ++i)
    {
        shader << "(2)";
    }
    EXPECT_FALSE(compile(shader.str()));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("array has too many dimensions"));
}

TEST_F(ParseTest, DeeplyNestedWhileStatementsNoCrash)
{
    mShaderSpec = SH_WEBGL2_SPEC;
    std::ostringstream shader;
    shader << R"(#version 300 es
void main() {
)";
    for (int i = 0; i < 1700; ++i)
    {
        shader << " while(true)";
    }
    shader << "; }";
    EXPECT_FALSE(compile(shader.str()));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("statement is too deeply nested"));
}

TEST_F(ParseTest, DeeplyNestedForStatementsNoCrash)
{
    mShaderSpec = SH_WEBGL2_SPEC;
    std::ostringstream shader;
    shader << R"(#version 300 es
void main() {
)";
    for (int i = 0; i < 1700; ++i)
    {
        shader << " for(int i = 0; i < 10; i++)";
    }
    shader << "; }";
    EXPECT_FALSE(compile(shader.str()));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("statement is too deeply nested"));
}

TEST_F(ParseTest, DeeplyNestedDoWhileStatementsNoCrash)
{
    mShaderSpec = SH_WEBGL2_SPEC;
    std::ostringstream shader;
    shader << R"(#version 300 es
void main() {
)";
    for (int i = 0; i < 1700; ++i)
    {
        shader << " do {";
    }
    for (int i = 0; i < 1700; ++i)
    {
        shader << "} while(true);";
    }
    shader << "}";
    EXPECT_FALSE(compile(shader.str()));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("statement is too deeply nested"));
}

TEST_F(ParseTest, DeeplyNestedSwitchStatementsNoCrash)
{
    mShaderSpec = SH_WEBGL2_SPEC;
    std::ostringstream shader;
    shader << R"(#version 300 es
void main() {
)";
    for (int i = 0; i < 1700; ++i)
    {
        shader << " switch(1) { default: int i=0;";
    }
    for (int i = 0; i < 1700; ++i)
    {
        shader << "}";
    }
    shader << "}";
    EXPECT_FALSE(compile(shader.str()));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("statement is too deeply nested"));
}

TEST_F(ParseTest, ManyChainedUnaryExpressionsNoCrash)
{
    mCompileOptions.limitExpressionComplexity = true;
    mShaderSpec                               = SH_WEBGL2_SPEC;
    std::ostringstream shader;
    shader << R"(#version 300 es
precision mediump float;
void main() {
  int iterations=0;)";
    for (int i = 0; i < 6000; ++i)
    {
        shader << "~";
    }
    shader << R"(++iterations;
}
)";
    EXPECT_FALSE(compile(shader.str()));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("Expression too complex"));
}

TEST_F(ParseTest, ManyChainedAssignmentsNoCrash)
{
    mCompileOptions.limitExpressionComplexity = true;
    mShaderSpec                               = SH_WEBGL2_SPEC;
    std::ostringstream shader;
    shader << R"(#version 300 es
void main() {
    int c = 0;
)";
    for (int i = 0; i < 3750; ++i)
    {
        shader << "c=\n";
    }
    shader << "c+1; }";
    EXPECT_FALSE(compile(shader.str()));
    EXPECT_TRUE(foundErrorInIntermediateTree());
    EXPECT_TRUE(foundInIntermediateTree("Expression too complex"));
}
