//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DebugShaderPrecision_test.cpp:
//   Tests for writing the code for shader precision emulation.
//

#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "gtest/gtest.h"
#include "tests/test_utils/compiler_test.h"

using namespace sh;

class DebugShaderPrecisionTest : public MatchOutputCodeTest
{
  public:
    DebugShaderPrecisionTest() : MatchOutputCodeTest(GL_FRAGMENT_SHADER, 0, SH_ESSL_OUTPUT)
    {
        addOutputType(SH_GLSL_COMPATIBILITY_OUTPUT);
#if defined(ANGLE_ENABLE_HLSL)
        addOutputType(SH_HLSL_4_1_OUTPUT);
#endif
        getResources()->WEBGL_debug_shader_precision = 1;
    }

  protected:
    bool foundInAllGLSLCode(const char *str)
    {
        return foundInCode(SH_GLSL_COMPATIBILITY_OUTPUT, str) && foundInCode(SH_ESSL_OUTPUT, str);
    }

    bool foundInHLSLCode(const char *stringToFind) const
    {
#if defined(ANGLE_ENABLE_HLSL)
        return foundInCode(SH_HLSL_4_1_OUTPUT, stringToFind);
#else
        return true;
#endif
    }

    bool foundInHLSLCodeRegex(const char *regexToFind) const
    {
#if defined(ANGLE_ENABLE_HLSL)
        return foundInCodeRegex(SH_HLSL_4_1_OUTPUT, std::regex(regexToFind));
#else
        return true;
#endif
    }
};

class NoDebugShaderPrecisionTest : public MatchOutputCodeTest
{
  public:
    NoDebugShaderPrecisionTest()
        : MatchOutputCodeTest(GL_FRAGMENT_SHADER, 0, SH_GLSL_COMPATIBILITY_OUTPUT)
    {}
};

TEST_F(DebugShaderPrecisionTest, RoundingFunctionsDefined)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform float u;\n"
        "void main() {\n"
        "   gl_FragColor = vec4(u);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInESSLCode("highp float angle_frm(in highp float"));
    ASSERT_TRUE(foundInESSLCode("highp vec2 angle_frm(in highp vec2"));
    ASSERT_TRUE(foundInESSLCode("highp vec3 angle_frm(in highp vec3"));
    ASSERT_TRUE(foundInESSLCode("highp vec4 angle_frm(in highp vec4"));
    ASSERT_TRUE(foundInESSLCode("highp mat2 angle_frm(in highp mat2"));
    ASSERT_TRUE(foundInESSLCode("highp mat3 angle_frm(in highp mat3"));
    ASSERT_TRUE(foundInESSLCode("highp mat4 angle_frm(in highp mat4"));

    ASSERT_TRUE(foundInESSLCode("highp float angle_frl(in highp float"));
    ASSERT_TRUE(foundInESSLCode("highp vec2 angle_frl(in highp vec2"));
    ASSERT_TRUE(foundInESSLCode("highp vec3 angle_frl(in highp vec3"));
    ASSERT_TRUE(foundInESSLCode("highp vec4 angle_frl(in highp vec4"));
    ASSERT_TRUE(foundInESSLCode("highp mat2 angle_frl(in highp mat2"));
    ASSERT_TRUE(foundInESSLCode("highp mat3 angle_frl(in highp mat3"));
    ASSERT_TRUE(foundInESSLCode("highp mat4 angle_frl(in highp mat4"));

    ASSERT_TRUE(foundInGLSLCode("float angle_frm(in float"));
    ASSERT_TRUE(foundInGLSLCode("vec2 angle_frm(in vec2"));
    ASSERT_TRUE(foundInGLSLCode("vec3 angle_frm(in vec3"));
    ASSERT_TRUE(foundInGLSLCode("vec4 angle_frm(in vec4"));
    ASSERT_TRUE(foundInGLSLCode("mat2 angle_frm(in mat2"));
    ASSERT_TRUE(foundInGLSLCode("mat3 angle_frm(in mat3"));
    ASSERT_TRUE(foundInGLSLCode("mat4 angle_frm(in mat4"));

    ASSERT_TRUE(foundInGLSLCode("float angle_frl(in float"));
    ASSERT_TRUE(foundInGLSLCode("vec2 angle_frl(in vec2"));
    ASSERT_TRUE(foundInGLSLCode("vec3 angle_frl(in vec3"));
    ASSERT_TRUE(foundInGLSLCode("vec4 angle_frl(in vec4"));
    ASSERT_TRUE(foundInGLSLCode("mat2 angle_frl(in mat2"));
    ASSERT_TRUE(foundInGLSLCode("mat3 angle_frl(in mat3"));
    ASSERT_TRUE(foundInGLSLCode("mat4 angle_frl(in mat4"));

    ASSERT_TRUE(foundInHLSLCode("float1 angle_frm(float1"));
    ASSERT_TRUE(foundInHLSLCode("float2 angle_frm(float2"));
    ASSERT_TRUE(foundInHLSLCode("float3 angle_frm(float3"));
    ASSERT_TRUE(foundInHLSLCode("float4 angle_frm(float4"));
    ASSERT_TRUE(foundInHLSLCode("float2x2 angle_frm(float2x2"));
    ASSERT_TRUE(foundInHLSLCode("float3x3 angle_frm(float3x3"));
    ASSERT_TRUE(foundInHLSLCode("float4x4 angle_frm(float4x4"));

    ASSERT_TRUE(foundInHLSLCode("float1 angle_frl(float1"));
    ASSERT_TRUE(foundInHLSLCode("float2 angle_frl(float2"));
    ASSERT_TRUE(foundInHLSLCode("float3 angle_frl(float3"));
    ASSERT_TRUE(foundInHLSLCode("float4 angle_frl(float4"));
    ASSERT_TRUE(foundInHLSLCode("float2x2 angle_frl(float2x2"));
    ASSERT_TRUE(foundInHLSLCode("float3x3 angle_frl(float3x3"));
    ASSERT_TRUE(foundInHLSLCode("float4x4 angle_frl(float4x4"));

    // Check that ESSL 3.00 rounding functions for non-square matrices are not defined.
    ASSERT_TRUE(notFoundInCode("mat2x"));
    ASSERT_TRUE(notFoundInCode("mat3x"));
    ASSERT_TRUE(notFoundInCode("mat4x"));
}

// Test that all ESSL 3.00 shaders get rounding function definitions for non-square matrices.
TEST_F(DebugShaderPrecisionTest, NonSquareMatrixRoundingFunctionsDefinedES3)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform float u;\n"
        "out vec4 my_FragColor;\n"
        "void main() {\n"
        "   my_FragColor = vec4(u);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInESSLCode("highp mat2x3 angle_frm(in highp mat2x3"));
    ASSERT_TRUE(foundInESSLCode("highp mat2x4 angle_frm(in highp mat2x4"));
    ASSERT_TRUE(foundInESSLCode("highp mat3x2 angle_frm(in highp mat3x2"));
    ASSERT_TRUE(foundInESSLCode("highp mat3x4 angle_frm(in highp mat3x4"));
    ASSERT_TRUE(foundInESSLCode("highp mat4x2 angle_frm(in highp mat4x2"));
    ASSERT_TRUE(foundInESSLCode("highp mat4x3 angle_frm(in highp mat4x3"));

    ASSERT_TRUE(foundInESSLCode("highp mat2x3 angle_frl(in highp mat2x3"));
    ASSERT_TRUE(foundInESSLCode("highp mat2x4 angle_frl(in highp mat2x4"));
    ASSERT_TRUE(foundInESSLCode("highp mat3x2 angle_frl(in highp mat3x2"));
    ASSERT_TRUE(foundInESSLCode("highp mat3x4 angle_frl(in highp mat3x4"));
    ASSERT_TRUE(foundInESSLCode("highp mat4x2 angle_frl(in highp mat4x2"));
    ASSERT_TRUE(foundInESSLCode("highp mat4x3 angle_frl(in highp mat4x3"));

    ASSERT_TRUE(foundInGLSLCode("mat2x3 angle_frm(in mat2x3"));
    ASSERT_TRUE(foundInGLSLCode("mat2x4 angle_frm(in mat2x4"));
    ASSERT_TRUE(foundInGLSLCode("mat3x2 angle_frm(in mat3x2"));
    ASSERT_TRUE(foundInGLSLCode("mat3x4 angle_frm(in mat3x4"));
    ASSERT_TRUE(foundInGLSLCode("mat4x2 angle_frm(in mat4x2"));
    ASSERT_TRUE(foundInGLSLCode("mat4x3 angle_frm(in mat4x3"));

    ASSERT_TRUE(foundInGLSLCode("mat2x3 angle_frl(in mat2x3"));
    ASSERT_TRUE(foundInGLSLCode("mat2x4 angle_frl(in mat2x4"));
    ASSERT_TRUE(foundInGLSLCode("mat3x2 angle_frl(in mat3x2"));
    ASSERT_TRUE(foundInGLSLCode("mat3x4 angle_frl(in mat3x4"));
    ASSERT_TRUE(foundInGLSLCode("mat4x2 angle_frl(in mat4x2"));
    ASSERT_TRUE(foundInGLSLCode("mat4x3 angle_frl(in mat4x3"));

    ASSERT_TRUE(foundInHLSLCode("float2x3 angle_frm(float2x3"));
    ASSERT_TRUE(foundInHLSLCode("float2x4 angle_frm(float2x4"));
    ASSERT_TRUE(foundInHLSLCode("float3x2 angle_frm(float3x2"));
    ASSERT_TRUE(foundInHLSLCode("float3x4 angle_frm(float3x4"));
    ASSERT_TRUE(foundInHLSLCode("float4x2 angle_frm(float4x2"));
    ASSERT_TRUE(foundInHLSLCode("float4x3 angle_frm(float4x3"));

    ASSERT_TRUE(foundInHLSLCode("float2x3 angle_frl(float2x3"));
    ASSERT_TRUE(foundInHLSLCode("float2x4 angle_frl(float2x4"));
    ASSERT_TRUE(foundInHLSLCode("float3x2 angle_frl(float3x2"));
    ASSERT_TRUE(foundInHLSLCode("float3x4 angle_frl(float3x4"));
    ASSERT_TRUE(foundInHLSLCode("float4x2 angle_frl(float4x2"));
    ASSERT_TRUE(foundInHLSLCode("float4x3 angle_frl(float4x3"));
}

TEST_F(DebugShaderPrecisionTest, PragmaDisablesEmulation)
{
    const std::string &shaderString =
        "#pragma webgl_debug_shader_precision(off)\n"
        "precision mediump float;\n"
        "uniform float u;\n"
        "void main() {\n"
        "   gl_FragColor = vec4(u);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(notFoundInCode("angle_frm"));
    const std::string &shaderStringPragmaOn =
        "#pragma webgl_debug_shader_precision(on)\n"
        "precision mediump float;\n"
        "uniform float u;\n"
        "void main() {\n"
        "   gl_FragColor = vec4(u);\n"
        "}\n";
    compile(shaderStringPragmaOn);
    ASSERT_TRUE(foundInCode("angle_frm"));
}

// Emulation can't be toggled on for only a part of a shader.
// Only the last pragma in the shader has an effect.
TEST_F(DebugShaderPrecisionTest, MultiplePragmas)
{
    const std::string &shaderString =
        "#pragma webgl_debug_shader_precision(off)\n"
        "precision mediump float;\n"
        "uniform float u;\n"
        "void main() {\n"
        "   gl_FragColor = vec4(u);\n"
        "}\n"
        "#pragma webgl_debug_shader_precision(on)\n";
    compile(shaderString);
    ASSERT_TRUE(foundInCode("angle_frm"));
}

TEST_F(NoDebugShaderPrecisionTest, HelpersWrittenOnlyWithExtension)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform float u;\n"
        "void main() {\n"
        "   gl_FragColor = vec4(u);\n"
        "}\n";
    compile(shaderString);
    ASSERT_FALSE(foundInCode("angle_frm"));
}

TEST_F(NoDebugShaderPrecisionTest, PragmaHasEffectsOnlyWithExtension)
{
    const std::string &shaderString =
        "#pragma webgl_debug_shader_precision(on)\n"
        "precision mediump float;\n"
        "uniform float u;\n"
        "void main() {\n"
        "   gl_FragColor = vec4(u);\n"
        "}\n";
    compile(shaderString);
    ASSERT_FALSE(foundInCode("angle_frm"));
}

TEST_F(DebugShaderPrecisionTest, DeclarationsAndConstants)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 f;\n"
        "uniform float uu, uu2;\n"
        "varying float vv, vv2;\n"
        "float gg = 0.0, gg2;\n"
        "void main() {\n"
        "   float aa = 0.0, aa2;\n"
        "   gl_FragColor = f;\n"
        "}\n";
    compile(shaderString);
    // Declarations or constants should not have rounding inserted around them
    ASSERT_TRUE(notFoundInCode("angle_frm(0"));
    // GLSL output
    ASSERT_TRUE(notFoundInCode("angle_frm(_uuu"));
    ASSERT_TRUE(notFoundInCode("angle_frm(_uvv"));
    ASSERT_TRUE(notFoundInCode("angle_frm(_ugg"));
    ASSERT_TRUE(notFoundInCode("angle_frm(_uaa"));
    // HLSL output
    ASSERT_TRUE(notFoundInCode("angle_frm(_uu"));
    ASSERT_TRUE(notFoundInCode("angle_frm(_vv"));
    ASSERT_TRUE(notFoundInCode("angle_frm(_gg"));
    ASSERT_TRUE(notFoundInCode("angle_frm(_aa"));
}

// Test that expressions that are part of initialization have rounding.
TEST_F(DebugShaderPrecisionTest, InitializerRounding)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform float u;\n"
        "void main() {\n"
        "   float a = u;\n"
        "   gl_FragColor = vec4(a);\n"
        "}\n";
    compile(shaderString);
    // An expression that's part of initialization should have rounding
    ASSERT_TRUE(foundInAllGLSLCode("angle_frm(_uu)"));
    ASSERT_TRUE(foundInHLSLCode("angle_frm(_u)"));
}

// Test that compound additions have rounding in the GLSL translations.
TEST_F(DebugShaderPrecisionTest, CompoundAddFunction)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u;\n"
        "uniform vec4 u2;\n"
        "void main() {\n"
        "   vec4 v = u;\n"
        "   v += u2;\n"
        "   gl_FragColor = v;\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(
        foundInESSLCode("highp vec4 angle_compound_add_frm(inout highp vec4 x, in highp vec4 y) {\n"
                        "    x = angle_frm(angle_frm(x) + y);"));
    ASSERT_TRUE(
        foundInGLSLCode("vec4 angle_compound_add_frm(inout vec4 x, in vec4 y) {\n"
                        "    x = angle_frm(angle_frm(x) + y);"));
    ASSERT_TRUE(
        foundInHLSLCode("float4 angle_compound_add_frm(inout float4 x, in float4 y) {\n"
                        "    x = angle_frm(angle_frm(x) + y);"));
    ASSERT_TRUE(foundInAllGLSLCode("angle_compound_add_frm(_uv, angle_frm(_uu2));"));
    ASSERT_TRUE(foundInHLSLCodeRegex(R"(angle_compound_add_frm\(_v(\d)*, angle_frm\(_u2\)\);)"));
    ASSERT_TRUE(notFoundInCode("+="));
}

TEST_F(DebugShaderPrecisionTest, CompoundSubFunction)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u;\n"
        "uniform vec4 u2;\n"
        "void main() {\n"
        "   vec4 v = u;\n"
        "   v -= u2;\n"
        "   gl_FragColor = v;\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(
        foundInESSLCode("highp vec4 angle_compound_sub_frm(inout highp vec4 x, in highp vec4 y) {\n"
                        "    x = angle_frm(angle_frm(x) - y);"));
    ASSERT_TRUE(
        foundInGLSLCode("vec4 angle_compound_sub_frm(inout vec4 x, in vec4 y) {\n"
                        "    x = angle_frm(angle_frm(x) - y);"));
    ASSERT_TRUE(
        foundInHLSLCode("float4 angle_compound_sub_frm(inout float4 x, in float4 y) {\n"
                        "    x = angle_frm(angle_frm(x) - y);"));
    ASSERT_TRUE(foundInAllGLSLCode("angle_compound_sub_frm(_uv, angle_frm(_uu2));"));
    ASSERT_TRUE(foundInHLSLCodeRegex("angle_compound_sub_frm\\(_v(\\d)*, angle_frm\\(_u2\\)\\);"));
    ASSERT_TRUE(notFoundInCode("-="));
}

TEST_F(DebugShaderPrecisionTest, CompoundDivFunction)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u;\n"
        "uniform vec4 u2;\n"
        "void main() {\n"
        "   vec4 v = u;\n"
        "   v /= u2;\n"
        "   gl_FragColor = v;\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(
        foundInESSLCode("highp vec4 angle_compound_div_frm(inout highp vec4 x, in highp vec4 y) {\n"
                        "    x = angle_frm(angle_frm(x) / y);"));
    ASSERT_TRUE(
        foundInGLSLCode("vec4 angle_compound_div_frm(inout vec4 x, in vec4 y) {\n"
                        "    x = angle_frm(angle_frm(x) / y);"));
    ASSERT_TRUE(
        foundInHLSLCode("float4 angle_compound_div_frm(inout float4 x, in float4 y) {\n"
                        "    x = angle_frm(angle_frm(x) / y);"));
    ASSERT_TRUE(foundInAllGLSLCode("angle_compound_div_frm(_uv, angle_frm(_uu2));"));
    ASSERT_TRUE(foundInHLSLCodeRegex("angle_compound_div_frm\\(_v(\\d)*, angle_frm\\(_u2\\)\\);"));
    ASSERT_TRUE(notFoundInCode("/="));
}

TEST_F(DebugShaderPrecisionTest, CompoundMulFunction)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u;\n"
        "uniform vec4 u2;\n"
        "void main() {\n"
        "   vec4 v = u;\n"
        "   v *= u2;\n"
        "   gl_FragColor = v;\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(
        foundInESSLCode("highp vec4 angle_compound_mul_frm(inout highp vec4 x, in highp vec4 y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(
        foundInGLSLCode("vec4 angle_compound_mul_frm(inout vec4 x, in vec4 y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(
        foundInHLSLCode("float4 angle_compound_mul_frm(inout float4 x, in float4 y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(foundInAllGLSLCode("angle_compound_mul_frm(_uv, angle_frm(_uu2));"));
    ASSERT_TRUE(foundInHLSLCodeRegex("angle_compound_mul_frm\\(_v(\\d)*, angle_frm\\(_u2\\)\\);"));
    ASSERT_TRUE(notFoundInCode("*="));
}

TEST_F(DebugShaderPrecisionTest, CompoundAddVectorPlusScalarFunction)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u;\n"
        "uniform float u2;\n"
        "void main() {\n"
        "   vec4 v = u;\n"
        "   v += u2;\n"
        "   gl_FragColor = v;\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInESSLCode(
        "highp vec4 angle_compound_add_frm(inout highp vec4 x, in highp float y) {\n"
        "    x = angle_frm(angle_frm(x) + y);"));
    ASSERT_TRUE(
        foundInGLSLCode("vec4 angle_compound_add_frm(inout vec4 x, in float y) {\n"
                        "    x = angle_frm(angle_frm(x) + y);"));
    ASSERT_TRUE(
        foundInHLSLCode("float4 angle_compound_add_frm(inout float4 x, in float y) {\n"
                        "    x = angle_frm(angle_frm(x) + y);"));
    ASSERT_TRUE(foundInAllGLSLCode("angle_compound_add_frm(_uv, angle_frm(_uu2));"));
    ASSERT_TRUE(foundInHLSLCodeRegex("angle_compound_add_frm\\(_v(\\d)*, angle_frm\\(_u2\\)\\);"));
    ASSERT_TRUE(notFoundInCode("+="));
}

TEST_F(DebugShaderPrecisionTest, CompoundMatrixTimesMatrixFunction)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform mat4 u;\n"
        "uniform mat4 u2;\n"
        "void main() {\n"
        "   mat4 m = u;\n"
        "   m *= u2;\n"
        "   gl_FragColor = m[0];\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(
        foundInESSLCode("highp mat4 angle_compound_mul_frm(inout highp mat4 x, in highp mat4 y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(
        foundInGLSLCode("mat4 angle_compound_mul_frm(inout mat4 x, in mat4 y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(
        foundInHLSLCode("float4x4 angle_compound_mul_frm(inout float4x4 x, in float4x4 y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(foundInAllGLSLCode("angle_compound_mul_frm(_um, angle_frm(_uu2));"));
    ASSERT_TRUE(foundInHLSLCodeRegex("angle_compound_mul_frm\\(_m(\\d)*, angle_frm\\(_u2\\)\\);"));
    ASSERT_TRUE(notFoundInCode("*="));
}

// Test that compound multiplying a non-square matrix with another matrix gets translated into an
// angle_compound_mul function call.
TEST_F(DebugShaderPrecisionTest, CompoundNonSquareMatrixTimesMatrixFunction)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform mat2x4 u;\n"
        "uniform mat2 u2;\n"
        "out vec4 my_FragColor;\n"
        "void main() {\n"
        "   mat2x4 m = u;\n"
        "   m *= u2;\n"
        "   my_FragColor = m[0];\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInESSLCode(
        "highp mat2x4 angle_compound_mul_frm(inout highp mat2x4 x, in highp mat2 y) {\n"
        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(
        foundInGLSLCode("mat2x4 angle_compound_mul_frm(inout mat2x4 x, in mat2 y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(
        foundInHLSLCode("float2x4 angle_compound_mul_frm(inout float2x4 x, in float2x2 y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(foundInAllGLSLCode("angle_compound_mul_frm(_um, angle_frm(_uu2));"));
    ASSERT_TRUE(foundInHLSLCodeRegex("angle_compound_mul_frm\\(_m(\\d)*, angle_frm\\(_u2\\)\\);"));
    ASSERT_TRUE(notFoundInCode("*="));
}

TEST_F(DebugShaderPrecisionTest, CompoundMatrixTimesScalarFunction)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform mat4 u;\n"
        "uniform float u2;\n"
        "void main() {\n"
        "   mat4 m = u;\n"
        "   m *= u2;\n"
        "   gl_FragColor = m[0];\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInESSLCode(
        "highp mat4 angle_compound_mul_frm(inout highp mat4 x, in highp float y) {\n"
        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(
        foundInGLSLCode("mat4 angle_compound_mul_frm(inout mat4 x, in float y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(
        foundInHLSLCode("float4x4 angle_compound_mul_frm(inout float4x4 x, in float y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(foundInAllGLSLCode("angle_compound_mul_frm(_um, angle_frm(_uu2));"));
    ASSERT_TRUE(foundInHLSLCodeRegex("angle_compound_mul_frm\\(_m(\\d)*, angle_frm\\(_u2\\)\\);"));
    ASSERT_TRUE(notFoundInCode("*="));
}

TEST_F(DebugShaderPrecisionTest, CompoundVectorTimesMatrixFunction)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u;\n"
        "uniform mat4 u2;\n"
        "void main() {\n"
        "   vec4 v = u;\n"
        "   v *= u2;\n"
        "   gl_FragColor = v;\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(
        foundInESSLCode("highp vec4 angle_compound_mul_frm(inout highp vec4 x, in highp mat4 y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(
        foundInGLSLCode("vec4 angle_compound_mul_frm(inout vec4 x, in mat4 y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(
        foundInHLSLCode("float4 angle_compound_mul_frm(inout float4 x, in float4x4 y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(foundInAllGLSLCode("angle_compound_mul_frm(_uv, angle_frm(_uu2));"));
    ASSERT_TRUE(foundInHLSLCodeRegex("angle_compound_mul_frm\\(_v(\\d)*, angle_frm\\(_u2\\)\\);"));
    ASSERT_TRUE(notFoundInCode("*="));
}

TEST_F(DebugShaderPrecisionTest, CompoundVectorTimesScalarFunction)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u;\n"
        "uniform float u2;\n"
        "void main() {\n"
        "   vec4 v = u;\n"
        "   v *= u2;\n"
        "   gl_FragColor = v;\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInESSLCode(
        "highp vec4 angle_compound_mul_frm(inout highp vec4 x, in highp float y) {\n"
        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(
        foundInGLSLCode("vec4 angle_compound_mul_frm(inout vec4 x, in float y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(
        foundInHLSLCode("float4 angle_compound_mul_frm(inout float4 x, in float y) {\n"
                        "    x = angle_frm(angle_frm(x) * y);"));
    ASSERT_TRUE(foundInAllGLSLCode("angle_compound_mul_frm(_uv, angle_frm(_uu2));"));
    ASSERT_TRUE(foundInHLSLCodeRegex("angle_compound_mul_frm\\(_v(\\d)*, angle_frm\\(_u2\\)\\);"));
    ASSERT_TRUE(notFoundInCode("*="));
}

TEST_F(DebugShaderPrecisionTest, BinaryMathRounding)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u1;\n"
        "uniform vec4 u2;\n"
        "uniform vec4 u3;\n"
        "uniform vec4 u4;\n"
        "uniform vec4 u5;\n"
        "void main() {\n"
        "   vec4 v1 = u1 + u2;\n"
        "   vec4 v2 = u2 - u3;\n"
        "   vec4 v3 = u3 * u4;\n"
        "   vec4 v4 = u4 / u5;\n"
        "   vec4 v5;\n"
        "   vec4 v6 = (v5 = u5);\n"
        "   gl_FragColor = v1 + v2 + v3 + v4;\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInAllGLSLCode("v1 = angle_frm((angle_frm(_uu1) + angle_frm(_uu2)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v2 = angle_frm((angle_frm(_uu2) - angle_frm(_uu3)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v3 = angle_frm((angle_frm(_uu3) * angle_frm(_uu4)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v4 = angle_frm((angle_frm(_uu4) / angle_frm(_uu5)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v6 = angle_frm((_uv5 = angle_frm(_uu5)))"));

    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v1(\\d)* = angle_frm\\(\\(angle_frm\\(_u1\\) \\+ angle_frm\\(_u2\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v2(\\d)* = angle_frm\\(\\(angle_frm\\(_u2\\) - angle_frm\\(_u3\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v3(\\d)* = angle_frm\\(\\(angle_frm\\(_u3\\) \\* angle_frm\\(_u4\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v4(\\d)* = angle_frm\\(\\(angle_frm\\(_u4\\) / angle_frm\\(_u5\\)\\)\\)"));
    ASSERT_TRUE(
        foundInHLSLCodeRegex("v6(\\d)* = angle_frm\\(\\(_v5(\\d)* = angle_frm\\(_u5\\)\\)\\)"));
}

TEST_F(DebugShaderPrecisionTest, BuiltInMathFunctionRounding)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u1;\n"
        "uniform vec4 u2;\n"
        "uniform vec4 u3;\n"
        "uniform float uf;\n"
        "uniform float uf2;\n"
        "uniform vec3 uf31;\n"
        "uniform vec3 uf32;\n"
        "uniform mat4 um1;\n"
        "uniform mat4 um2;\n"
        "void main() {\n"
        "   vec4 v1 = radians(u1);\n"
        "   vec4 v2 = degrees(u1);\n"
        "   vec4 v3 = sin(u1);\n"
        "   vec4 v4 = cos(u1);\n"
        "   vec4 v5 = tan(u1);\n"
        "   vec4 v6 = asin(u1);\n"
        "   vec4 v7 = acos(u1);\n"
        "   vec4 v8 = atan(u1);\n"
        "   vec4 v9 = atan(u1, u2);\n"
        "   vec4 v10 = pow(u1, u2);\n"
        "   vec4 v11 = exp(u1);\n"
        "   vec4 v12 = log(u1);\n"
        "   vec4 v13 = exp2(u1);\n"
        "   vec4 v14 = log2(u1);\n"
        "   vec4 v15 = sqrt(u1);\n"
        "   vec4 v16 = inversesqrt(u1);\n"
        "   vec4 v17 = abs(u1);\n"
        "   vec4 v18 = sign(u1);\n"
        "   vec4 v19 = floor(u1);\n"
        "   vec4 v20 = ceil(u1);\n"
        "   vec4 v21 = fract(u1);\n"
        "   vec4 v22 = mod(u1, uf);\n"
        "   vec4 v23 = mod(u1, u2);\n"
        "   vec4 v24 = min(u1, uf);\n"
        "   vec4 v25 = min(u1, u2);\n"
        "   vec4 v26 = max(u1, uf);\n"
        "   vec4 v27 = max(u1, u2);\n"
        "   vec4 v28 = clamp(u1, u2, u3);\n"
        "   vec4 v29 = clamp(u1, uf, uf2);\n"
        "   vec4 v30 = mix(u1, u2, u3);\n"
        "   vec4 v31 = mix(u1, u2, uf);\n"
        "   vec4 v32 = step(u1, u2);\n"
        "   vec4 v33 = step(uf, u1);\n"
        "   vec4 v34 = smoothstep(u1, u2, u3);\n"
        "   vec4 v35 = smoothstep(uf, uf2, u1);\n"
        "   vec4 v36 = normalize(u1);\n"
        "   vec4 v37 = faceforward(u1, u2, u3);\n"
        "   vec4 v38 = reflect(u1, u2);\n"
        "   vec4 v39 = refract(u1, u2, uf);\n"

        "   float f1 = length(u1);\n"
        "   float f2 = distance(u1, u2);\n"
        "   float f3 = dot(u1, u2);\n"
        "   vec3 vf31 = cross(uf31, uf32);\n"
        "   mat4 m1 = matrixCompMult(um1, um2);\n"

        "   gl_FragColor = v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 +"
        "v11 + v12 + v13 + v14 + v15 + v16 + v17 + v18 + v19 + v20 +"
        "v21 + v22 + v23 + v24 + v25 + v26 + v27 + v28 + v29 + v30 +"
        "v31 + v32 + v33 + v34 + v35 + v36 + v37 + v38 + v39 +"
        "vec4(f1, f2, f3, 0.0) + vec4(vf31, 0.0) + m1[0];\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInAllGLSLCode("v1 = angle_frm(radians(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v2 = angle_frm(degrees(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v3 = angle_frm(sin(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v4 = angle_frm(cos(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v5 = angle_frm(tan(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v6 = angle_frm(asin(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v7 = angle_frm(acos(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v8 = angle_frm(atan(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v9 = angle_frm(atan(angle_frm(_uu1), angle_frm(_uu2)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v10 = angle_frm(pow(angle_frm(_uu1), angle_frm(_uu2)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v11 = angle_frm(exp(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v12 = angle_frm(log(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v13 = angle_frm(exp2(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v14 = angle_frm(log2(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v15 = angle_frm(sqrt(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v16 = angle_frm(inversesqrt(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v17 = angle_frm(abs(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v18 = angle_frm(sign(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v19 = angle_frm(floor(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v20 = angle_frm(ceil(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v21 = angle_frm(fract(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v22 = angle_frm(mod(angle_frm(_uu1), angle_frm(_uuf)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v23 = angle_frm(mod(angle_frm(_uu1), angle_frm(_uu2)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v24 = angle_frm(min(angle_frm(_uu1), angle_frm(_uuf)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v25 = angle_frm(min(angle_frm(_uu1), angle_frm(_uu2)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v26 = angle_frm(max(angle_frm(_uu1), angle_frm(_uuf)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v27 = angle_frm(max(angle_frm(_uu1), angle_frm(_uu2)))"));
    ASSERT_TRUE(foundInAllGLSLCode(
        "v28 = angle_frm(clamp(angle_frm(_uu1), angle_frm(_uu2), angle_frm(_uu3)))"));
    ASSERT_TRUE(foundInAllGLSLCode(
        "v29 = angle_frm(clamp(angle_frm(_uu1), angle_frm(_uuf), angle_frm(_uuf2)))"));
    ASSERT_TRUE(foundInAllGLSLCode(
        "v30 = angle_frm(mix(angle_frm(_uu1), angle_frm(_uu2), angle_frm(_uu3)))"));
    ASSERT_TRUE(foundInAllGLSLCode(
        "v31 = angle_frm(mix(angle_frm(_uu1), angle_frm(_uu2), angle_frm(_uuf)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v32 = angle_frm(step(angle_frm(_uu1), angle_frm(_uu2)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v33 = angle_frm(step(angle_frm(_uuf), angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode(
        "v34 = angle_frm(smoothstep(angle_frm(_uu1), angle_frm(_uu2), angle_frm(_uu3)))"));
    ASSERT_TRUE(foundInAllGLSLCode(
        "v35 = angle_frm(smoothstep(angle_frm(_uuf), angle_frm(_uuf2), angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v36 = angle_frm(normalize(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode(
        "v37 = angle_frm(faceforward(angle_frm(_uu1), angle_frm(_uu2), angle_frm(_uu3)))"));
    ASSERT_TRUE(foundInAllGLSLCode("v38 = angle_frm(reflect(angle_frm(_uu1), angle_frm(_uu2)))"));
    ASSERT_TRUE(foundInAllGLSLCode(
        "v39 = angle_frm(refract(angle_frm(_uu1), angle_frm(_uu2), angle_frm(_uuf)))"));

    ASSERT_TRUE(foundInAllGLSLCode("f1 = angle_frm(length(angle_frm(_uu1)))"));
    ASSERT_TRUE(foundInAllGLSLCode("f2 = angle_frm(distance(angle_frm(_uu1), angle_frm(_uu2)))"));
    ASSERT_TRUE(foundInAllGLSLCode("f3 = angle_frm(dot(angle_frm(_uu1), angle_frm(_uu2)))"));
    ASSERT_TRUE(
        foundInAllGLSLCode("vf31 = angle_frm(cross(angle_frm(_uuf31), angle_frm(_uuf32)))"));
    ASSERT_TRUE(
        foundInAllGLSLCode("m1 = angle_frm(matrixCompMult(angle_frm(_uum1), angle_frm(_uum2)))"));

    ASSERT_TRUE(foundInHLSLCodeRegex("v1(\\d)* = angle_frm\\(radians\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v2(\\d)* = angle_frm\\(degrees\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v3(\\d)* = angle_frm\\(sin\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v4(\\d)* = angle_frm\\(cos\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v5(\\d)* = angle_frm\\(tan\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v6(\\d)* = angle_frm\\(asin\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v7(\\d)* = angle_frm\\(acos\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v8(\\d)* = angle_frm\\(atan\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v9(\\d)* = angle_frm\\(atan_emu\\(angle_frm\\(_u1\\), angle_frm\\(_u2\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v10(\\d)* = angle_frm\\(pow\\(angle_frm\\(_u1\\), angle_frm\\(_u2\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v11(\\d)* = angle_frm\\(exp\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v12(\\d)* = angle_frm\\(log\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v13(\\d)* = angle_frm\\(exp2\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v14(\\d)* = angle_frm\\(log2\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v15(\\d)* = angle_frm\\(sqrt\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v16(\\d)* = angle_frm\\(rsqrt\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v17(\\d)* = angle_frm\\(abs\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v18(\\d)* = angle_frm\\(sign\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v19(\\d)* = angle_frm\\(floor\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v20(\\d)* = angle_frm\\(ceil\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v21(\\d)* = angle_frm\\(frac\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v22(\\d)* = angle_frm\\(mod_emu\\(angle_frm\\(_u1\\), angle_frm\\(_uf\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v23(\\d)* = angle_frm\\(mod_emu\\(angle_frm\\(_u1\\), angle_frm\\(_u2\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v24(\\d)* = angle_frm\\(min\\(angle_frm\\(_u1\\), angle_frm\\(_uf\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v25(\\d)* = angle_frm\\(min\\(angle_frm\\(_u1\\), angle_frm\\(_u2\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v26(\\d)* = angle_frm\\(max\\(angle_frm\\(_u1\\), angle_frm\\(_uf\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v27(\\d)* = angle_frm\\(max\\(angle_frm\\(_u1\\), angle_frm\\(_u2\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v28(\\d)* = angle_frm\\(clamp\\(angle_frm\\(_u1\\), angle_frm\\(_u2\\), "
        "angle_frm\\(_u3\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v29(\\d)* = angle_frm\\(clamp\\(angle_frm\\(_u1\\), angle_frm\\(_uf\\), "
        "angle_frm\\(_uf2\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v30(\\d)* = angle_frm\\(lerp\\(angle_frm\\(_u1\\), angle_frm\\(_u2\\), "
        "angle_frm\\(_u3\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v31(\\d)* = angle_frm\\(lerp\\(angle_frm\\(_u1\\), angle_frm\\(_u2\\), "
        "angle_frm\\(_uf\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v32(\\d)* = angle_frm\\(step\\(angle_frm\\(_u1\\), angle_frm\\(_u2\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v33(\\d)* = angle_frm\\(step\\(angle_frm\\(_uf\\), angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(
        foundInHLSLCodeRegex("v34(\\d)* = angle_frm\\(smoothstep\\(angle_frm\\(_u1\\), "
                             "angle_frm\\(_u2\\), angle_frm\\(_u3\\)\\)\\)"));
    ASSERT_TRUE(
        foundInHLSLCodeRegex("v35(\\d)* = angle_frm\\(smoothstep\\(angle_frm\\(_uf\\), "
                             "angle_frm\\(_uf2\\), angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(
        foundInHLSLCodeRegex("v36(\\d)* = angle_frm\\(normalize\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(
        foundInHLSLCodeRegex("v37(\\d)* = angle_frm\\(faceforward_emu\\(angle_frm\\(_u1\\), "
                             "angle_frm\\(_u2\\), angle_frm\\(_u3\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v38(\\d)* = angle_frm\\(reflect\\(angle_frm\\(_u1\\), angle_frm\\(_u2\\)\\)\\)"));
    ASSERT_TRUE(
        foundInHLSLCodeRegex("v39(\\d)* = angle_frm\\(refract\\(angle_frm\\(_u1\\), "
                             "angle_frm\\(_u2\\), angle_frm\\(_uf\\)\\)\\)"));

    ASSERT_TRUE(foundInHLSLCodeRegex("f1(\\d)* = angle_frm\\(length\\(angle_frm\\(_u1\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "f2(\\d)* = angle_frm\\(distance\\(angle_frm\\(_u1\\), angle_frm\\(_u2\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "f3(\\d)* = angle_frm\\(dot\\(angle_frm\\(_u1\\), angle_frm\\(_u2\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "vf31(\\d)* = angle_frm\\(cross\\(angle_frm\\(_uf31\\), angle_frm\\(_uf32\\)\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "m1(\\d)* = angle_frm\\(\\(angle_frm\\(_um1\\) \\* angle_frm\\(_um2\\)\\)\\)"));
}

TEST_F(DebugShaderPrecisionTest, BuiltInRelationalFunctionRounding)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u1;\n"
        "uniform vec4 u2;\n"
        "void main() {\n"
        "   bvec4 bv1 = lessThan(u1, u2);\n"
        "   bvec4 bv2 = lessThanEqual(u1, u2);\n"
        "   bvec4 bv3 = greaterThan(u1, u2);\n"
        "   bvec4 bv4 = greaterThanEqual(u1, u2);\n"
        "   bvec4 bv5 = equal(u1, u2);\n"
        "   bvec4 bv6 = notEqual(u1, u2);\n"
        "   gl_FragColor = vec4(bv1) + vec4(bv2) + vec4(bv3) + vec4(bv4) + vec4(bv5) + vec4(bv6);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInAllGLSLCode("bv1 = lessThan(angle_frm(_uu1), angle_frm(_uu2))"));
    ASSERT_TRUE(foundInAllGLSLCode("bv2 = lessThanEqual(angle_frm(_uu1), angle_frm(_uu2))"));
    ASSERT_TRUE(foundInAllGLSLCode("bv3 = greaterThan(angle_frm(_uu1), angle_frm(_uu2))"));
    ASSERT_TRUE(foundInAllGLSLCode("bv4 = greaterThanEqual(angle_frm(_uu1), angle_frm(_uu2))"));
    ASSERT_TRUE(foundInAllGLSLCode("bv5 = equal(angle_frm(_uu1), angle_frm(_uu2))"));
    ASSERT_TRUE(foundInAllGLSLCode("bv6 = notEqual(angle_frm(_uu1), angle_frm(_uu2))"));

    ASSERT_TRUE(foundInHLSLCodeRegex("bv1(\\d)* = \\(angle_frm\\(_u1\\) < angle_frm\\(_u2\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("bv2(\\d)* = \\(angle_frm\\(_u1\\) <= angle_frm\\(_u2\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("bv3(\\d)* = \\(angle_frm\\(_u1\\) > angle_frm\\(_u2\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("bv4(\\d)* = \\(angle_frm\\(_u1\\) >= angle_frm\\(_u2\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("bv5(\\d)* = \\(angle_frm\\(_u1\\) == angle_frm\\(_u2\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("bv6(\\d)* = \\(angle_frm\\(_u1\\) != angle_frm\\(_u2\\)\\)"));
}

TEST_F(DebugShaderPrecisionTest, ConstructorRounding)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "precision mediump int;\n"
        "uniform float u1;\n"
        "uniform float u2;\n"
        "uniform lowp float u3;\n"
        "uniform float u4;\n"
        "uniform ivec4 uiv;\n"
        "void main() {\n"
        "   vec4 v1 = vec4(u1, u2, u3, u4);\n"
        "   vec4 v2 = vec4(uiv);\n"
        "   gl_FragColor = v1 + v2;\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInAllGLSLCode("v1 = angle_frm(vec4(_uu1, _uu2, angle_frl(_uu3), _uu4))"));
    ASSERT_TRUE(foundInAllGLSLCode("v2 = angle_frm(vec4(_uuiv))"));

    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v1(\\d)* = angle_frm\\(vec4_ctor\\(_u1, _u2, angle_frl\\(_u3\\), _u4\\)\\)"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v2(\\d)* = angle_frm\\(vec4_ctor\\(_uiv\\)\\)"));
}

TEST_F(DebugShaderPrecisionTest, StructConstructorNoRounding)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "struct S { mediump vec4 a; };\n"
        "uniform vec4 u;\n"
        "void main() {\n"
        "   S s = S(u);\n"
        "   gl_FragColor = s.a;\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInAllGLSLCode("s = _uS(angle_frm(_uu))"));
    ASSERT_TRUE(foundInHLSLCodeRegex("s(\\d)* = _S_ctor\\(angle_frm\\(_u\\)\\)"));
    ASSERT_TRUE(notFoundInCode("angle_frm(_uS"));  // GLSL
    ASSERT_TRUE(notFoundInCode("angle_frm(_S"));   // HLSL
}

TEST_F(DebugShaderPrecisionTest, SwizzleRounding)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u;\n"
        "void main() {\n"
        "   vec4 v = u.xyxy;"
        "   gl_FragColor = v;\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInAllGLSLCode("v = angle_frm(_uu).xyxy"));
    ASSERT_TRUE(foundInHLSLCodeRegex("v(\\d)* = angle_frm\\(_u\\)\\.xyxy"));
}

TEST_F(DebugShaderPrecisionTest, BuiltInTexFunctionRounding)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "precision lowp sampler2D;\n"
        "uniform vec2 u;\n"
        "uniform sampler2D s;\n"
        "void main() {\n"
        "   lowp vec4 v = texture2D(s, u);\n"
        "   gl_FragColor = v;\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInAllGLSLCode("v = angle_frl(texture2D(_us, angle_frm(_uu)))"));
    ASSERT_TRUE(
        foundInHLSLCodeRegex("v(\\d)* = angle_frl\\(gl_texture2D\\(_s, angle_frm\\(_u\\)\\)\\)"));
}

TEST_F(DebugShaderPrecisionTest, FunctionCallParameterQualifiersFromDefinition)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u1;\n"
        "uniform vec4 u2;\n"
        "uniform vec4 u3;\n"
        "uniform vec4 u4;\n"
        "uniform vec4 u5;\n"
        "vec4 add(in vec4 x, in vec4 y) {\n"
        "   return x + y;\n"
        "}\n"
        "void compound_add(inout vec4 x, in vec4 y) {\n"
        "   x = x + y;\n"
        "}\n"
        "void add_to_last(in vec4 x, in vec4 y, out vec4 z) {\n"
        "   z = x + y;\n"
        "}\n"
        "void main() {\n"
        "   vec4 v = add(u1, u2);\n"
        "   compound_add(v, u3);\n"
        "   vec4 v2;\n"
        "   add_to_last(u4, u5, v2);\n"
        "   gl_FragColor = v + v2;\n"
        "}\n";
    compile(shaderString);
    // Note that this is not optimal code, there are redundant frm calls.
    // However, getting the implementation working when other operations
    // are nested within function calls would be tricky if to get right
    // otherwise.
    // Test in parameters
    ASSERT_TRUE(foundInAllGLSLCode("v = _uadd(angle_frm(_uu1), angle_frm(_uu2))"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v(\\d)* = f_add_float4_float4\\(angle_frm\\(_u1\\), angle_frm\\(_u2\\)\\)"));
    // Test inout parameter
    ASSERT_TRUE(foundInAllGLSLCode("_ucompound_add(_uv, angle_frm(_uu3))"));
    ASSERT_TRUE(
        foundInHLSLCodeRegex("compound_add_float4_float4\\(_v(\\d)*, angle_frm\\(_u3\\)\\)"));
    // Test out parameter
    ASSERT_TRUE(foundInAllGLSLCode("_uadd_to_last(angle_frm(_uu4), angle_frm(_uu5), _uv2)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "add_to_last_float4_float4_float4\\(angle_frm\\(_u4\\), angle_frm\\(_u5\\), _v2(\\d)*\\)"));
}

TEST_F(DebugShaderPrecisionTest, FunctionCallParameterQualifiersFromPrototype)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u1;\n"
        "uniform vec4 u2;\n"
        "uniform vec4 u3;\n"
        "uniform vec4 u4;\n"
        "uniform vec4 u5;\n"
        "vec4 add(in vec4 x, in vec4 y);\n"
        "void compound_add(inout vec4 x, in vec4 y);\n"
        "void add_to_last(in vec4 x, in vec4 y, out vec4 z);\n"
        "void main() {\n"
        "   vec4 v = add(u1, u2);\n"
        "   compound_add(v, u3);\n"
        "   vec4 v2;\n"
        "   add_to_last(u4, u5, v2);\n"
        "   gl_FragColor = v + v2;\n"
        "}\n"
        "vec4 add(in vec4 x, in vec4 y) {\n"
        "   return x + y;\n"
        "}\n"
        "void compound_add(inout vec4 x, in vec4 y) {\n"
        "   x = x + y;\n"
        "}\n"
        "void add_to_last(in vec4 x, in vec4 y, out vec4 z) {\n"
        "   z = x + y;\n"
        "}\n";
    compile(shaderString);
    // Test in parameters
    ASSERT_TRUE(foundInAllGLSLCode("v = _uadd(angle_frm(_uu1), angle_frm(_uu2))"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v(\\d)* = f_add_float4_float4\\(angle_frm\\(_u1\\), angle_frm\\(_u2\\)\\)"));
    // Test inout parameter
    ASSERT_TRUE(foundInAllGLSLCode("_ucompound_add(_uv, angle_frm(_uu3))"));
    ASSERT_TRUE(
        foundInHLSLCodeRegex("compound_add_float4_float4\\(_v(\\d)*, angle_frm\\(_u3\\)\\)"));
    // Test out parameter
    ASSERT_TRUE(foundInAllGLSLCode("add_to_last(angle_frm(_uu4), angle_frm(_uu5), _uv2)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "add_to_last_float4_float4_float4\\(angle_frm\\(_u4\\), angle_frm\\(_u5\\), _v2(\\d)*\\)"));
}

TEST_F(DebugShaderPrecisionTest, NestedFunctionCalls)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u1;\n"
        "uniform vec4 u2;\n"
        "uniform vec4 u3;\n"
        "vec4 add(in vec4 x, in vec4 y) {\n"
        "   return x + y;\n"
        "}\n"
        "vec4 compound_add(inout vec4 x, in vec4 y) {\n"
        "   x = x + y;\n"
        "   return x;\n"
        "}\n"
        "void main() {\n"
        "   vec4 v = u1;\n"
        "   vec4 v2 = add(compound_add(v, u2), fract(u3));\n"
        "   gl_FragColor = v + v2;\n"
        "}\n";
    compile(shaderString);
    // Test nested calls
    ASSERT_TRUE(foundInAllGLSLCode(
        "v2 = _uadd(_ucompound_add(_uv, angle_frm(_uu2)), angle_frm(fract(angle_frm(_uu3))))"));
    ASSERT_TRUE(foundInHLSLCodeRegex(
        "v2(\\d)* = f_add_float4_float4\\(f_compound_add_float4_float4\\(_v(\\d)*, "
        "angle_frm\\(_u2\\)\\), "
        "angle_frm\\(frac\\(angle_frm\\(_u3\\)\\)\\)\\)"));
}

// Test that code inside an index of a function out parameter gets processed.
TEST_F(DebugShaderPrecisionTest, OpInIndexOfFunctionOutParameter)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "void foo(out vec4 f) { f.x = 0.0; }\n"
        "uniform float u2;\n"
        "void main() {\n"
        "   vec4 v[2];\n"
        "   foo(v[int(exp2(u2))]);\n"
        "   gl_FragColor = v[0];\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInAllGLSLCode("angle_frm(exp2(angle_frm(_uu2)))"));
    ASSERT_TRUE(foundInHLSLCode("angle_frm(exp2(angle_frm(_u2)))"));
}

// Test that code inside an index of an l-value gets processed.
TEST_F(DebugShaderPrecisionTest, OpInIndexOfLValue)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform vec4 u1;\n"
        "uniform float u2;\n"
        "void main() {\n"
        "   vec4 v[2];\n"
        "   v[int(exp2(u2))] = u1;\n"
        "   gl_FragColor = v[0];\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInAllGLSLCode("angle_frm(exp2(angle_frm(_uu2)))"));
    ASSERT_TRUE(foundInHLSLCode("angle_frm(exp2(angle_frm(_u2)))"));
}

// Test that the out parameter of modf doesn't get rounded
TEST_F(DebugShaderPrecisionTest, ModfOutParameter)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform float u;\n"
        "out vec4 my_FragColor;\n"
        "void main() {\n"
        "   float o;\n"
        "   float f = modf(u, o);\n"
        "   my_FragColor = vec4(f, o, 0, 1);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInAllGLSLCode("modf(angle_frm(_uu), _uo)"));
    ASSERT_TRUE(foundInHLSLCodeRegex(R"(modf\(angle_frm\(_u\), _o(\d)*)"));
}

#if defined(ANGLE_ENABLE_HLSL)
// Tests precision emulation with HLSL 3.0 output -- should error gracefully.
TEST(DebugShaderPrecisionNegativeTest, HLSL3Unsupported)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform float u;\n"
        "void main() {\n"
        "   gl_FragColor = vec4(u);\n"
        "}\n";
    std::string infoLog;
    std::string translatedCode;
    ShBuiltInResources resources;
    sh::InitBuiltInResources(&resources);
    resources.WEBGL_debug_shader_precision = 1;
    ASSERT_FALSE(compileTestShader(GL_FRAGMENT_SHADER, SH_GLES3_SPEC, SH_HLSL_3_0_OUTPUT,
                                   shaderString, &resources, 0, &translatedCode, &infoLog));
}
#endif  // defined(ANGLE_ENABLE_HLSL)

// Test that compound assignment inside an expression compiles correctly. This is a test for a bug
// where incorrect type information on the compound assignment call node caused an assert to trigger
// in the debug build.
TEST_F(DebugShaderPrecisionTest, CompoundAssignmentInsideExpression)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main() {\n"
        "   float f = 0.0;\n"
        "   my_FragColor = vec4(abs(f += 1.0), 0, 0, 1);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInAllGLSLCode("abs(angle_compound_add_frm(_uf, 1.0))"));
}

// Test that having rounded values inside the right hand side of logical or doesn't trigger asserts
// in HLSL output.
TEST_F(DebugShaderPrecisionTest, RoundedValueOnRightSideOfLogicalOr)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "uniform float u1, u2;\n"
        "void main() {\n"
        "   my_FragColor = vec4(u1 == 0.0 || u2 == 0.0);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInHLSLCode("angle_frm(_u2) == 0.0"));
}
