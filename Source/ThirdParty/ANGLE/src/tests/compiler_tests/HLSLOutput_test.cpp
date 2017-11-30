//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HLSLOutput_test.cpp:
//   Tests for HLSL output.
//

#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "gtest/gtest.h"
#include "tests/test_utils/compiler_test.h"

using namespace sh;

class HLSLOutputTest : public MatchOutputCodeTest
{
  public:
    HLSLOutputTest() : MatchOutputCodeTest(GL_FRAGMENT_SHADER, 0, SH_HLSL_4_1_OUTPUT) {}
};

class HLSL30VertexOutputTest : public MatchOutputCodeTest
{
  public:
    HLSL30VertexOutputTest() : MatchOutputCodeTest(GL_VERTEX_SHADER, 0, SH_HLSL_3_0_OUTPUT) {}
};

// Test that having dynamic indexing of a vector inside the right hand side of logical or doesn't
// trigger asserts in HLSL output.
TEST_F(HLSLOutputTest, DynamicIndexingOfVectorOnRightSideOfLogicalOr)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 my_FragColor;\n"
        "uniform int u1;\n"
        "void main() {\n"
        "   bvec4 v = bvec4(true, true, true, false);\n"
        "   my_FragColor = vec4(v[u1 + 1] || v[u1]);\n"
        "}\n";
    compile(shaderString);
}

// Test that rewriting else blocks in a function that returns a struct doesn't use the struct name
// without a prefix.
TEST_F(HLSL30VertexOutputTest, RewriteElseBlockReturningStruct)
{
    const std::string &shaderString =
        "struct foo\n"
        "{\n"
        "    float member;\n"
        "};\n"
        "uniform bool b;\n"
        "foo getFoo()\n"
        "{\n"
        "    if (b)\n"
        "    {\n"
        "        return foo(0.0);\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        return foo(1.0);\n"
        "    }\n"
        "}\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(getFoo().member);\n"
        "}\n";
    compile(shaderString);
    EXPECT_TRUE(foundInCode("_foo"));
    EXPECT_FALSE(foundInCode("(foo)"));
    EXPECT_FALSE(foundInCode(" foo"));
}
