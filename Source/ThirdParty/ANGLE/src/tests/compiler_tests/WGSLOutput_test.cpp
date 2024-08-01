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

        vec4 doFoo(Foo foo, float zw);

        vec4 doFoo(Foo foo, float zw)
        {
            // foo.x = foo.y;
            return vec4(foo.x, foo.y, zw, zw);
        }

        Foo returnFoo(Foo foo) {
          return foo;
        }

        float returnFloat(float x) {
          return x;
        }

        float takeArgs(vec2 x, float y) {
          return y;
        }

        void main()
        {
            Foo foo;
            // Struct field accesses.
            foo.x = 2.0;
            foo.y = 2.0;
            // Complicated constUnion should be emitted correctly.
            foo.multiArray = vec3[][](
              vec3[](
                vec3(1.0, 2.0, 3.0),
                vec3(1.0, 2.0, 3.0),
                vec3(1.0, 2.0, 3.0)),
              vec3[](
                vec3(4.0, 5.0, 6.0),
                vec3(4.0, 5.0, 6.0),
                vec3(4.0, 5.0, 6.0)
              )
            );
            int arrIndex = 1;
            // Access an array index with a constant index.
            float f = foo.multiArray[0][1].x;
            // Access an array index with a non-const index, should clamp by default.
            float f2 = foo.multiArray[0][arrIndex].x;
            gl_FragDepth = f + f2;
            doFoo(returnFoo(foo), returnFloat(3.0));
            takeArgs(vec2(1.0, 2.0), foo.x);
            returnFloat(doFoo(foo, 7.0 + 9.0).x);
        })";
    const std::string &outputString =
        R"(_uoutColor : vec4<f32>;

struct _uFoo
{
  _ux : f32,
  _uy : f32,
  _umultiArray : array<array<vec3<f32>, 3>, 2>,
  _uaMatrix : mat3x3<f32>,
};

fn _udoFoo(_ufoo : _uFoo, _uzw : f32) -> vec4<f32>;

fn _udoFoo(_ufoo : _uFoo, _uzw : f32) -> vec4<f32>
{
  return vec4<f32>((_ufoo)._ux, (_ufoo)._uy, _uzw, _uzw);
}

fn _ureturnFoo(_ufoo : _uFoo) -> _uFoo
{
  return _ufoo;
}

fn _ureturnFloat(_ux : f32) -> f32
{
  return _ux;
}

fn _utakeArgs(_ux : vec2<f32>, _uy : f32) -> f32
{
  return _uy;
}

fn _umain()
{
  _ufoo : _uFoo;
  ((_ufoo)._ux) = (2.0f);
  ((_ufoo)._uy) = (2.0f);
  ((_ufoo)._umultiArray) = (array<array<vec3<f32>, 3>, 2>(array<vec3<f32>, 3>(vec3<f32>(1.0f, 2.0f, 3.0f), vec3<f32>(1.0f, 2.0f, 3.0f), vec3<f32>(1.0f, 2.0f, 3.0f)), array<vec3<f32>, 3>(vec3<f32>(4.0f, 5.0f, 6.0f), vec3<f32>(4.0f, 5.0f, 6.0f), vec3<f32>(4.0f, 5.0f, 6.0f))));
  _uarrIndex : i32 = (1i);
  _uf : f32 = (((((_ufoo)._umultiArray)[0i])[1i]).x);
  _uf2 : f32 = (((((_ufoo)._umultiArray)[0i])[clamp((_uarrIndex), 0, 2)]).x);
  (gl_FragDepth) = ((_uf) + (_uf2));
  _udoFoo(_ureturnFoo(_ufoo), _ureturnFloat(3.0f));
  _utakeArgs(vec2<f32>(1.0f, 2.0f), (_ufoo)._ux);
  _ureturnFloat((_udoFoo(_ufoo, 16.0f)).x);
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, ControlFlow)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        int ifElseDemo() {
          int x = 5;
          if (x == 5) {
            return 6;
          } else if (x == 6) {
            return 7;
          } else {
            return 8;
          }
        }

        void switchDemo() {
          int x = 5;
          switch (x) {
          case 5:
          case 6:
            discard;
          case 7: {
            return;
          }
          case 8:
          case 9:
            {
              x = 7;
            }
            return;
          default:
            return;
          }
        }

        void forLoopDemo() {
          for (int i = 0; i < 5; i++) {
            if (i == 4) {
              break;
            } else if (i == 5) {
              continue;
            }
          }
        }

        void whileLoopDemo() {
          int i = 0;
          while (i < 5) {
            i++;
          }

          do {
            i++;
          } while (i < 5);
        }

        void main()
        {
          ifElseDemo();
          switchDemo();
          forLoopDemo();
          whileLoopDemo();
        })";
    const std::string &outputString =
        R"(
fn _uifElseDemo() -> i32
{
  _ux : i32 = (5i);
  if ((_ux) == (5i))
  {
    return 6i;
  }
  else
  {
    if ((_ux) == (6i))
    {
      return 7i;
    }
    else
    {
      return 8i;
    }
  }
}

fn _uswitchDemo()
{
  _ux : i32 = (5i);
  switch _ux
  {
    case 5i, 6i:
    {
      discard;
    }
    case 7i:
    {
      {
        return;
      }
    }
    case 8i, 9i:
    {
      {
        (_ux) = (7i);
      }
      return;
    }
    case default:
    {
      return;
    }
  }
}

fn _uforLoopDemo()
{
  for (_ui : i32 = (0i); (_ui) < (5i); (_ui)++)
  {
    if ((_ui) == (4i))
    {
      break;
    }
    else
    {
      if ((_ui) == (5i))
      {
        continue;
      }
    }
  }
}

fn _uwhileLoopDemo()
{
  _ui : i32 = (0i);
  while ((_ui) < (5i))
  {
    (_ui)++;
  }
  loop {
    {
      (_ui)++;
    }
    if (!((_ui) < (5i)) { break; }
  }
}

fn _umain()
{
  _uifElseDemo();
  _uswitchDemo();
  _uforLoopDemo();
  _uwhileLoopDemo();
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}
