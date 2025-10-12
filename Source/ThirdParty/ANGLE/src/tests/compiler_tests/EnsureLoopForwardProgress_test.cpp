//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EnsureLoopForwardProgress_test.cpp:
//   Tests that loops make forward progress.
//

#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tests/test_utils/compiler_test.h"

namespace
{
using namespace sh;
using namespace testing;

class EnsureLoopForwardProgressTest : public MatchOutputCodeTest
{
  public:
    EnsureLoopForwardProgressTest() : MatchOutputCodeTest(GL_FRAGMENT_SHADER, SH_ESSL_OUTPUT)
    {
        ShCompileOptions defaultCompileOptions          = {};
        defaultCompileOptions.ensureLoopForwardProgress = true;
        defaultCompileOptions.validateAST               = true;
        setDefaultCompileOptions(defaultCompileOptions);
    }
};

TEST_F(EnsureLoopForwardProgressTest, FiniteForInitLessThanConstantPlusPlus)
{
    const char kShader[]   = R"(#version 300 es
void main() {
        for (highp int i = 0; i < 100; ++i) { }
})";
    const char kExpected[] = R"(#version 300 es
void main(){
  for (highp int _ui = 0; (_ui < 100); (++_ui))
  {
  }
}
)";
    compile(kShader);
    EXPECT_EQ(kExpected, outputCode(SH_ESSL_OUTPUT));
}

TEST_F(EnsureLoopForwardProgressTest, InfiniteForExample)
{
    const char kShader[]   = R"(#version 300 es
void main() {
  for (highp int i = 0; i < 100; i++) { i = 0; }
})";
    const char kExpected[] = R"(#version 300 es
void main(){
  for (highp int _ui = 0; (_ui < 100); (_ui++))
  {
    loopForwardProgress();
    {
      (_ui = 0);
    }
  }
}
)";
    compile(kShader);
    EXPECT_EQ(kExpected, outputCode(SH_ESSL_OUTPUT));
}

TEST_F(EnsureLoopForwardProgressTest, InfiniteNestedForExample)
{
    const char kShader[]   = R"(#version 300 es
void main() {
  for (highp int i = 0; i < 100; i++) { for (highp int j = 0; j < 100; j++) { j = 0; } i = 0; }
})";
    const char kExpected[] = R"(#version 300 es
void main(){
  for (highp int _ui = 0; (_ui < 100); (_ui++))
  {
    loopForwardProgress();
    {
      for (highp int _uj = 0; (_uj < 100); (_uj++))
      {
        loopForwardProgress();
        {
          (_uj = 0);
        }
      }
      (_ui = 0);
    }
  }
}
)";
    compile(kShader);
    EXPECT_EQ(kExpected, outputCode(SH_ESSL_OUTPUT));
}

TEST_F(EnsureLoopForwardProgressTest, FiniteFors)
{
    const char kShaderPrefix[] = R"(#version 300 es
precision highp int;
uniform int a;
uniform uint b;
void main() {

)";
    const char kShaderSuffix[] = "}\n";
    const char *tests[]{"int i = 101; for (; i < 10; i++) { }",
                        "int i = 101; for (; i < 10; i+=1) { }",
                        "int i = 101; for (; i < 10; i-=1) { }",
                        "for (int i = 0; i < 10; i++) { }",
                        "for (int i = 0; i < a; i++) { }",
                        "for (int i = 0; i < 100000/2; ++i) { }",
                        "for (uint i = 0u; i < 10u; i++) { }",
                        "for (uint i = 0u; i < b; i++) { }",
                        "for (uint i = 0u; i < 100000u/2u; ++i) { }",
                        "for (uint i = 0u; i < 4294967295u; ++i) { }",
                        "for (uint i = 10u; i > 1u+3u ; --i) { }",
                        "const int z = 7; for (int i = 0; i < z; i++) { }",
                        "for (int i = 0; i < 10; i++) { for (int j = 0; j < 1000; ++j) { }}"};
    for (auto &test : tests)
    {
        std::string shader = (std::stringstream() << kShaderPrefix << test << kShaderSuffix).str();
        compile(shader.c_str());
        std::string output = outputCode(SH_ESSL_OUTPUT);
        EXPECT_THAT(output, HasSubstr("void main(){"));
        EXPECT_THAT(output, Not(HasSubstr("loopForwardProgress();")))
            << "input: " << test << "output: " << output;
    }
}

TEST_F(EnsureLoopForwardProgressTest, InfiniteFors)
{
    const char kShaderPrefix[] = R"(#version 300 es
precision highp int;
uniform int a;
uniform uint b;
void main() {

)";
    const char kShaderSuffix[] = "}\n";
    const char *tests[]{"for (;;) { }",
                        "for (bool b = true; b; b = false) { }",
                        "for (int i = 0; i < 10;) { }",
                        "int i = 101; for (; i < 10; i+=2) { }",
                        "int i = 101; for (; i < 10; i-=2) { }",
                        "int z = 7; for (int i = 0; i < z; i++) { }",
                        "for (int i = 0; i < 10; i++) { i++; }",
                        "for (int i = 0; i < 10;) { i++; }",
                        "for (int i = 0; i < a/2; i++) { }",
                        "for (int i = 0; float(i) < 10e10; ++i) { }",
                        "for (int i = 0; i < 10; i++) { for (int j = 0; j < 1000; ++i) { }}",
                        "for (int i = 0; i != 1; i+=2) { }"};
    for (auto &test : tests)
    {
        std::string shader = (std::stringstream() << kShaderPrefix << test << kShaderSuffix).str();
        compile(shader.c_str());
        std::string output = outputCode(SH_ESSL_OUTPUT);
        EXPECT_THAT(output, HasSubstr("void main(){"));
        EXPECT_THAT(output, HasSubstr("loopForwardProgress();"))
            << "input: " << test << "output: " << output;
    }
}

}  // namespace