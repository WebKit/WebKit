//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PruneEmptyDeclarations_test.cpp:
//   Tests for pruning empty declarations.
//

#include "angle_gl.h"
#include "gtest/gtest.h"
#include "GLSLANG/ShaderLang.h"
#include "tests/test_utils/compiler_test.h"

namespace
{

class PruneEmptyDeclarationsTest : public MatchOutputCodeTest
{
  public:
    PruneEmptyDeclarationsTest()
        : MatchOutputCodeTest(GL_VERTEX_SHADER, 0, SH_GLSL_COMPATIBILITY_OUTPUT)
    {
    }
};

TEST_F(PruneEmptyDeclarationsTest, EmptyDeclarationStartsDeclaratorList)
{
    const std::string shaderString =
        "precision mediump float;\n"
        "uniform float u;\n"
        "void main()\n"
        "{\n"
        "   float, f;\n"
        "   gl_Position = vec4(u * f);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInCode("float f"));
    ASSERT_TRUE(notFoundInCode("float, f"));
}

TEST_F(PruneEmptyDeclarationsTest, EmptyStructDeclarationWithQualifiers)
{
    const std::string shaderString =
        "precision mediump float;\n"
        "const struct S { float f; };\n"
        "uniform S s;"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(s.f);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(foundInCode("struct S"));
    ASSERT_TRUE(foundInCode("uniform S"));
    ASSERT_TRUE(notFoundInCode("const struct S"));
}

}  // namespace
