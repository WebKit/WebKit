//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WGSLOutput_test.cpp:
//   Tests for corect WGSL translations.
//

#include <regex>
#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "gtest/gtest.h"
#include "tests/test_utils/compiler_test.h"

using namespace sh;

class WGSLVertexOutputTest : public MatchOutputCodeTest
{
  public:
    WGSLVertexOutputTest() : MatchOutputCodeTest(GL_VERTEX_SHADER, SH_WGSL_OUTPUT)
    {
        ShCompileOptions defaultCompileOptions = {};
        defaultCompileOptions.validateAST      = true;
        setDefaultCompileOptions(defaultCompileOptions);
    }
};

class WGSLOutputTest : public MatchOutputCodeTest
{
  public:
    WGSLOutputTest() : MatchOutputCodeTest(GL_FRAGMENT_SHADER, SH_WGSL_OUTPUT)
    {
        ShCompileOptions defaultCompileOptions = {};
        defaultCompileOptions.validateAST      = true;
        setDefaultCompileOptions(defaultCompileOptions);
    }
};

TEST_F(WGSLOutputTest, BasicTranslation)
{
    const std::string &shaderString =
        R"(#version 310 es
        precision highp float;

        out vec4 outColor;

        struct Foo {
            float x;
            float y;
            vec3 multiArray[2][3];
            mat3 aMatrix;
        };

        void doFoo(Foo foo, float zw);

        void doFoo(Foo foo, float zw)
        {
            foo.x = foo.y;
            outColor = vec4(foo.x, foo.y, zw, zw);
        }

        Foo returnFoo(Foo foo) {
          return foo;
        }

        float returnFloat(float x) {
          return x;
        }

        void main()
        {
            Foo foo;
            foo.x = 2.0;
            foo.y = 2.0;
            doFoo(returnFoo(foo), returnFloat(3.0));
        })";
    const std::string &outputString =
        R"(
_uoutColor : vec4<f32>;

struct _uFoo
{
  _ux : f32,
  _uy : f32,
  _umultiArray : array<array<vec3<f32>, 3>, 2>,
  _uaMatrix : mat3x3<f32>,
};

fn _udoFoo(_ufoo : _uFoo, _uzw : f32);

fn _udoFoo(_ufoo : _uFoo, _uzw : f32)
{
  ;
  ;
}

fn _ureturnFoo(_ufoo : _uFoo) -> _uFoo
{
  ;
}

fn _ureturnFloat(_ux : f32) -> f32
{
  ;
}

fn _umain()
{
  _ufoo : _uFoo;
  ;
  ;
  ;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}
