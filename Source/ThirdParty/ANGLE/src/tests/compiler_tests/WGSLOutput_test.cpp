//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WGSLOutput_test.cpp:
//   Tests for correct WGSL translations.
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

        getResources()->MaxDrawBuffers = 2;
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

            vec3 comp1 = vec3(0.0, 0.0, 0.0);
            float f3 = float(comp1 == foo.multiArray[0][0]);

            outColor = vec4(f3, 0.0, 0.0, 0.0);
        })";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Output_Global {
  _uoutColor : vec4<f32>,
  gl_FragDepth_ : f32,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _uoutColor : vec4<f32>,
  @builtin(frag_depth) gl_FragDepth_ : f32,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

struct _uFoo
{
  _ux : f32,
  _uy : f32,
  _umultiArray : array<array<vec3<f32>, 3>, 2>,
  _uaMatrix : mat3x3<f32>,
};

;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;
;

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
  var _ufoo : _uFoo;
  ((_ufoo)._ux) = (2.0f);
  ((_ufoo)._uy) = (2.0f);
  ((_ufoo)._umultiArray) = (array<array<vec3<f32>, 3>, 2>(array<vec3<f32>, 3>(vec3<f32>(1.0f, 2.0f, 3.0f), vec3<f32>(1.0f, 2.0f, 3.0f), vec3<f32>(1.0f, 2.0f, 3.0f)), array<vec3<f32>, 3>(vec3<f32>(4.0f, 5.0f, 6.0f), vec3<f32>(4.0f, 5.0f, 6.0f), vec3<f32>(4.0f, 5.0f, 6.0f))));
  var _uarrIndex : i32 = (1i);
  var _uf : f32 = (((((_ufoo)._umultiArray)[0i])[1i]).x);
  var _uf2 : f32 = (((((_ufoo)._umultiArray)[0i])[clamp((_uarrIndex), 0, 2)]).x);
  (ANGLE_output_global.gl_FragDepth_) = ((_uf) + (_uf2));
  _udoFoo(_ureturnFoo(_ufoo), _ureturnFloat(3.0f));
  _utakeArgs(vec2<f32>(1.0f, 2.0f), (_ufoo)._ux);
  _ureturnFloat((_udoFoo(_ufoo, 16.0f)).x);
  var _ucomp1 : vec3<f32> = (vec3<f32>(0.0f, 0.0f, 0.0f));
  var _uf3 : f32 = (f32(all((_ucomp1) == ((((_ufoo)._umultiArray)[0i])[0i]))));
  (ANGLE_output_global._uoutColor) = (vec4<f32>(_uf3, 0.0f, 0.0f, 0.0f));
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._uoutColor = ANGLE_output_global._uoutColor;
  ANGLE_output_annotated.gl_FragDepth_ = ANGLE_output_global.gl_FragDepth_;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, OverloadedFunctions)
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

        vec4 doFoo(Foo foo)
        {
            return vec4(foo.x, foo.y, 0.0, 0.0);
        }

        vec4 doFoo(Foo foo, float zw)
        {
            return vec4(foo.x, foo.y, zw, zw);
        }

        vec4 doFoo(Foo[2] foo, float zw)
        {
            return vec4(foo[0].x, foo[0].y, zw, zw);
        }

        vec4 doFoo(Foo[3] foo, float zw)
        {
            return vec4(foo[0].x, foo[0].y, zw, zw);
        }

        vec4 doFoo(Foo[2][2] foo, float zw, mat2x2 a)
        {
            return vec4(foo[0][0].x, foo[0][0].y, zw, zw);
        }

        vec4 doFoo(Foo[2][2] foo, float zw, mat2x2 a, bvec2 b)
        {
            return vec4(foo[0][0].x, foo[0][0].y, zw, zw);
        }

        vec4 doFoo(Foo[2][2] foo, float zw, mat2x2 a, ivec2 b)
        {
            return vec4(foo[0][0].x, foo[0][0].y, zw, zw);
        }

        vec4 doFoo(Foo[2][2] foo, float zw, mat2x2 a, uvec2 b)
        {
            return vec4(foo[0][0].x, foo[0][0].y, zw, zw);
        }

        void main()
        {
            Foo foo;
            doFoo(foo);
            doFoo(foo, 3.0);
            doFoo(Foo[2](foo, foo), 3.0);
            doFoo(Foo[3](foo, foo, foo), 3.0);
            doFoo(Foo[2][2](Foo[2](foo, foo), Foo[2](foo, foo)), 3.0,
              mat2x2(1.0));
            doFoo(Foo[2][2](Foo[2](foo, foo), Foo[2](foo, foo)), 3.0,
              mat2x2(1.0), bvec2(true, false));
            doFoo(Foo[2][2](Foo[2](foo, foo), Foo[2](foo, foo)), 3.0,
              mat2x2(1.0), ivec2(1, 2));
            doFoo(Foo[2][2](Foo[2](foo, foo), Foo[2](foo, foo)), 3.0,
              mat2x2(1.0), uvec2(1, 2));

            outColor = vec4(foo.x, 0.0, 0.0, 0.0);
        })";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Output_Global {
  _uoutColor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _uoutColor : vec4<f32>,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

struct _uFoo
{
  _ux : f32,
  _uy : f32,
  _umultiArray : array<array<vec3<f32>, 3>, 2>,
  _uaMatrix : mat3x3<f32>,
};

;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

fn _udoFoo(_ufoo : _uFoo) -> vec4<f32>
{
  return vec4<f32>((_ufoo)._ux, (_ufoo)._uy, 0.0f, 0.0f);
}

fn ANGLEfunc3006_udoFoo(_ufoo : _uFoo, _uzw : f32) -> vec4<f32>
{
  return vec4<f32>((_ufoo)._ux, (_ufoo)._uy, _uzw, _uzw);
}

fn ANGLEfunc3009_udoFoo(_ufoo : array<_uFoo, 2>, _uzw : f32) -> vec4<f32>
{
  return vec4<f32>(((_ufoo)[0i])._ux, ((_ufoo)[0i])._uy, _uzw, _uzw);
}

fn ANGLEfunc3012_udoFoo(_ufoo : array<_uFoo, 3>, _uzw : f32) -> vec4<f32>
{
  return vec4<f32>(((_ufoo)[0i])._ux, ((_ufoo)[0i])._uy, _uzw, _uzw);
}

fn ANGLEfunc3015_udoFoo(_ufoo : array<array<_uFoo, 2>, 2>, _uzw : f32, _ua : mat2x2<f32>) -> vec4<f32>
{
  return vec4<f32>((((_ufoo)[0i])[0i])._ux, (((_ufoo)[0i])[0i])._uy, _uzw, _uzw);
}

fn ANGLEfunc3019_udoFoo(_ufoo : array<array<_uFoo, 2>, 2>, _uzw : f32, _ua : mat2x2<f32>, _ub : vec2<bool>) -> vec4<f32>
{
  return vec4<f32>((((_ufoo)[0i])[0i])._ux, (((_ufoo)[0i])[0i])._uy, _uzw, _uzw);
}

fn ANGLEfunc3024_udoFoo(_ufoo : array<array<_uFoo, 2>, 2>, _uzw : f32, _ua : mat2x2<f32>, _ub : vec2<i32>) -> vec4<f32>
{
  return vec4<f32>((((_ufoo)[0i])[0i])._ux, (((_ufoo)[0i])[0i])._uy, _uzw, _uzw);
}

fn ANGLEfunc3029_udoFoo(_ufoo : array<array<_uFoo, 2>, 2>, _uzw : f32, _ua : mat2x2<f32>, _ub : vec2<u32>) -> vec4<f32>
{
  return vec4<f32>((((_ufoo)[0i])[0i])._ux, (((_ufoo)[0i])[0i])._uy, _uzw, _uzw);
}

fn _umain()
{
  var _ufoo : _uFoo;
  _udoFoo(_ufoo);
  ANGLEfunc3006_udoFoo(_ufoo, 3.0f);
  ANGLEfunc3009_udoFoo(array<_uFoo, 2>(_ufoo, _ufoo), 3.0f);
  ANGLEfunc3012_udoFoo(array<_uFoo, 3>(_ufoo, _ufoo, _ufoo), 3.0f);
  ANGLEfunc3015_udoFoo(array<array<_uFoo, 2>, 2>(array<_uFoo, 2>(_ufoo, _ufoo), array<_uFoo, 2>(_ufoo, _ufoo)), 3.0f, mat2x2<f32>(1.0f, 0.0f, 0.0f, 1.0f));
  ANGLEfunc3019_udoFoo(array<array<_uFoo, 2>, 2>(array<_uFoo, 2>(_ufoo, _ufoo), array<_uFoo, 2>(_ufoo, _ufoo)), 3.0f, mat2x2<f32>(1.0f, 0.0f, 0.0f, 1.0f), vec2<bool>(true, false));
  ANGLEfunc3024_udoFoo(array<array<_uFoo, 2>, 2>(array<_uFoo, 2>(_ufoo, _ufoo), array<_uFoo, 2>(_ufoo, _ufoo)), 3.0f, mat2x2<f32>(1.0f, 0.0f, 0.0f, 1.0f), vec2<i32>(1i, 2i));
  ANGLEfunc3029_udoFoo(array<array<_uFoo, 2>, 2>(array<_uFoo, 2>(_ufoo, _ufoo), array<_uFoo, 2>(_ufoo, _ufoo)), 3.0f, mat2x2<f32>(1.0f, 0.0f, 0.0f, 1.0f), vec2<u32>(1u, 2u));
  (ANGLE_output_global._uoutColor) = (vec4<f32>((_ufoo)._ux, 0.0f, 0.0f, 0.0f));
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._uoutColor = ANGLE_output_global._uoutColor;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, ModifyFunctionParameters)
{
    const std::string &shaderString =
        R"(#version 310 es
        precision highp float;

        out vec4 outColor;

        struct Foo {
            float x;
            float y;
        };

        vec4 doFoo(Foo foo, float zw)
        {
            foo.x = foo.y;
            return vec4(foo.x, foo.y, zw, zw);
        }

        float returnFloat(float x) {
          x += 5.0;
          return x;
        }

        float takeArgs(vec2 x, float y, float z) {
          y -= x.x;
          z -= 1.0;
          return y;
        }

        void main()
        {
            Foo foo;
            // Struct field accesses.
            foo.x = 2.0;
            foo.y = 2.0;

            doFoo(foo, returnFloat(3.0));
            takeArgs(vec2(1.0, 2.0), foo.x, 7.0);
            float f3 = returnFloat(doFoo(foo, 7.0 + 9.0).x);

            outColor = vec4(f3, 0.0, 0.0, 0.0);
        })";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Output_Global {
  _uoutColor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _uoutColor : vec4<f32>,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

struct _uFoo
{
  _ux : f32,
  _uy : f32,
};

;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

fn _udoFoo(_ufoo : _uFoo, _uzw : f32) -> vec4<f32>
{
  var sbc8 : _uFoo = (_ufoo);
  ((sbc8)._ux) = ((sbc8)._uy);
  return vec4<f32>((sbc8)._ux, (sbc8)._uy, _uzw, _uzw);
}

fn _ureturnFloat(_ux : f32) -> f32
{
  var sbc9 : f32 = (_ux);
  (sbc9) += (5.0f);
  return sbc9;
}

fn _utakeArgs(_ux : vec2<f32>, _uy : f32, _uz : f32) -> f32
{
  var sbca : f32 = (_uy);
  var sbcb : f32 = (_uz);
  (sbca) -= ((_ux).x);
  (sbcb) -= (1.0f);
  return sbca;
}

fn _umain()
{
  var _ufoo : _uFoo;
  ((_ufoo)._ux) = (2.0f);
  ((_ufoo)._uy) = (2.0f);
  _udoFoo(_ufoo, _ureturnFloat(3.0f));
  _utakeArgs(vec2<f32>(1.0f, 2.0f), (_ufoo)._ux, 7.0f);
  var _uf3 : f32 = (_ureturnFloat((_udoFoo(_ufoo, 16.0f)).x));
  (ANGLE_output_global._uoutColor) = (vec4<f32>(_uf3, 0.0f, 0.0f, 0.0f));
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._uoutColor = ANGLE_output_global._uoutColor;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, MultiElementSwizzle)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        in vec3 inVec;
        out vec4 outColor;

        void main()
        {
          outColor = vec4(0.0, 0.0, 0.0, 0.0);

          // Unimplemented or buggy:
          // outColor.xy += 0.25;
          // (outColor.xy + 0.25, outColor.yz) += 0.25;

          outColor.zx = vec2(1.0, 1.0);

          outColor.zx = inVec.xz;

          outColor.xy *= mat2(1.0, 0.0, 0.0, 1.0);
        })";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Input_Global {
  _uinVec : vec3<f32>,
};

var<private> ANGLE_input_global : ANGLE_Input_Global;

struct ANGLE_Input_Annotated {
  @location(@@@@@@) _uinVec : vec3<f32>,
};

struct ANGLE_Output_Global {
  _uoutColor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _uoutColor : vec4<f32>,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

;
;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

fn _umain()
{
  (ANGLE_output_global._uoutColor) = (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f));
  var sbbc : vec2<f32> = (vec2<f32>(1.0f, 1.0f));
  ((ANGLE_output_global._uoutColor).z) = ((sbbc).x);
  ((ANGLE_output_global._uoutColor).x) = ((sbbc).y);
  var sbbd : vec2<f32> = ((ANGLE_input_global._uinVec).xz);
  ((ANGLE_output_global._uoutColor).z) = ((sbbd).x);
  ((ANGLE_output_global._uoutColor).x) = ((sbbd).y);
  var sbbe : mat2x2<f32> = (mat2x2<f32>(1.0f, 0.0f, 0.0f, 1.0f));
  ((ANGLE_output_global._uoutColor).x) = ((((ANGLE_output_global._uoutColor).xy) * (sbbe)).x);
  ((ANGLE_output_global._uoutColor).y) = ((((ANGLE_output_global._uoutColor).xy) * (sbbe)).y);
}
@fragment
fn wgslMain(ANGLE_input_annotated : ANGLE_Input_Annotated) -> ANGLE_Output_Annotated
{
  ANGLE_input_global._uinVec = ANGLE_input_annotated._uinVec;
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._uoutColor = ANGLE_output_global._uoutColor;
  return ANGLE_output_annotated;
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
        R"(diagnostic(warning,derivative_uniformity);
@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

fn _uifElseDemo() -> i32
{
  var _ux : i32 = (5i);
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
  var _ux : i32 = (5i);
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
  for (var _ui : i32 = (0i); (_ui) < (5i); (_ui)++)
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
  var _ui : i32 = (0i);
  while ((_ui) < (5i))
  {
    (_ui)++;
  }
  loop {
    {
      (_ui)++;
    }
    continuing {
      break if !((_ui) < (5i));
    }
  }
}

fn _umain()
{
  _uifElseDemo();
  _uswitchDemo();
  _uforLoopDemo();
  _uwhileLoopDemo();
}
@fragment
fn wgslMain()
{
  _umain();
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, ChainedAssignment)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        in vec4 inColor;
        layout (location = 0) out vec4 color1;
        layout (location = 1) out vec4 color2;

        float globVar = 1.0;

        void main()
        {
          vec4 tempColor;
          if ((tempColor = inColor).x == 1.0) {
              color1 = color2 = inColor;
          }
        })";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
fn ANGLE_assignPriv_0(dest : ptr<private, vec4<f32>>, src : vec4<f32>) -> vec4<f32>  {
  *dest = src;
  return *dest;
}
fn ANGLE_assignFunc_0(dest : ptr<function, vec4<f32>>, src : vec4<f32>) -> vec4<f32>  {
  *dest = src;
  return *dest;
}
struct ANGLE_Input_Global {
  _uinColor : vec4<f32>,
};

var<private> ANGLE_input_global : ANGLE_Input_Global;

struct ANGLE_Input_Annotated {
  @location(@@@@@@) _uinColor : vec4<f32>,
};

struct ANGLE_Output_Global {
  _ucolor1 : vec4<f32>,
  _ucolor2 : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _ucolor1 : vec4<f32>,
  @location(@@@@@@) _ucolor2 : vec4<f32>,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

;
;
;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

fn _umain()
{
  var _utempColor : vec4<f32>;
  if (((ANGLE_assignFunc_0(&_utempColor, ANGLE_input_global._uinColor)).x) == (1.0f))
  {
    (ANGLE_output_global._ucolor1) = (ANGLE_assignPriv_0(&ANGLE_output_global._ucolor2, ANGLE_input_global._uinColor));
  }
}
@fragment
fn wgslMain(ANGLE_input_annotated : ANGLE_Input_Annotated) -> ANGLE_Output_Annotated
{
  ANGLE_input_global._uinColor = ANGLE_input_annotated._uinColor;
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._ucolor1 = ANGLE_output_global._ucolor1;
  ANGLE_output_annotated._ucolor2 = ANGLE_output_global._ucolor2;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, IncrementDecrement)
{

    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        out vec4 color;

        float globVar = 1.0;

        void main()
        {
          for (int i = 0; i < 5; i++) {
              color.x++;
          }

          uint i = 0u;
          while (i++ < 5u) {
              color.y++;
          }

          color++;

          ivec4 iv = ivec4(1,2,3,4);
          iv++;
          ++iv;
          color.x += float(iv.x);

          mat2x2 m = mat2x2(4);
          m++;

          color.xy *= m;

          mat2x2 m2 = m++;

          color.xy *= m2;

          color++;

          globVar++;

          color.x += globVar;
        })";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
fn ANGLE_preIncPriv_4(x : ptr<private, vec4<i32>>) -> vec4<i32> {
  (*x) += vec4<i32>(1);
  return *x;
}
fn ANGLE_preIncFunc_4(x : ptr<function, vec4<i32>>) -> vec4<i32> {
  (*x) += vec4<i32>(1);
  return *x;
}
fn ANGLE_postIncPriv_0(x : ptr<private, f32>) -> f32 {
  var old = *x;
  (*x) += f32(1);
  return old;
}
fn ANGLE_postIncFunc_0(x : ptr<function, f32>) -> f32 {
  var old = *x;
  (*x) += f32(1);
  return old;
}
fn ANGLE_postIncPriv_5(x : ptr<private, mat2x2<f32>>) -> mat2x2<f32> {
  var old = *x;
  (*x) += mat2x2<f32>(1, 1, 1, 1);
  return old;
}
fn ANGLE_postIncFunc_5(x : ptr<function, mat2x2<f32>>) -> mat2x2<f32> {
  var old = *x;
  (*x) += mat2x2<f32>(1, 1, 1, 1);
  return old;
}
fn ANGLE_postIncPriv_2(x : ptr<private, vec4<f32>>) -> vec4<f32> {
  var old = *x;
  (*x) += vec4<f32>(1);
  return old;
}
fn ANGLE_postIncFunc_2(x : ptr<function, vec4<f32>>) -> vec4<f32> {
  var old = *x;
  (*x) += vec4<f32>(1);
  return old;
}
fn ANGLE_postIncPriv_3(x : ptr<private, vec4<i32>>) -> vec4<i32> {
  var old = *x;
  (*x) += vec4<i32>(1);
  return old;
}
fn ANGLE_postIncFunc_3(x : ptr<function, vec4<i32>>) -> vec4<i32> {
  var old = *x;
  (*x) += vec4<i32>(1);
  return old;
}
fn ANGLE_postIncPriv_1(x : ptr<private, u32>) -> u32 {
  var old = *x;
  (*x) += u32(1);
  return old;
}
fn ANGLE_postIncFunc_1(x : ptr<function, u32>) -> u32 {
  var old = *x;
  (*x) += u32(1);
  return old;
}
struct ANGLE_Output_Global {
  _ucolor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _ucolor : vec4<f32>,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

;
var<private> _uglobVar : f32 = (1.0f);

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

fn _umain()
{
  for (var _ui : i32 = (0i); (_ui) < (5i); (_ui)++)
  {
    ANGLE_postIncFunc_0(&((ANGLE_output_global._ucolor).x));
  }
  var _ui : u32 = (0u);
  while ((ANGLE_postIncFunc_1(&(_ui))) < (5u))
  {
    ANGLE_postIncFunc_0(&((ANGLE_output_global._ucolor).y));
  }
  ANGLE_postIncPriv_2(&(ANGLE_output_global._ucolor));
  var _uiv : vec4<i32> = (vec4<i32>(1i, 2i, 3i, 4i));
  ANGLE_postIncFunc_3(&(_uiv));
  ANGLE_preIncFunc_4(&(_uiv));
  ((ANGLE_output_global._ucolor).x) += (f32((_uiv).x));
  var _um : mat2x2<f32> = (mat2x2<f32>(4.0f, 0.0f, 0.0f, 4.0f));
  ANGLE_postIncFunc_5(&(_um));
  var sbc1 : mat2x2<f32> = (_um);
  ((ANGLE_output_global._ucolor).x) = ((((ANGLE_output_global._ucolor).xy) * (sbc1)).x);
  ((ANGLE_output_global._ucolor).y) = ((((ANGLE_output_global._ucolor).xy) * (sbc1)).y);
  var _um2 : mat2x2<f32> = (ANGLE_postIncFunc_5(&(_um)));
  var sbc2 : mat2x2<f32> = (_um2);
  ((ANGLE_output_global._ucolor).x) = ((((ANGLE_output_global._ucolor).xy) * (sbc2)).x);
  ((ANGLE_output_global._ucolor).y) = ((((ANGLE_output_global._ucolor).xy) * (sbc2)).y);
  ANGLE_postIncPriv_2(&(ANGLE_output_global._ucolor));
  ANGLE_postIncPriv_0(&(_uglobVar));
  ((ANGLE_output_global._ucolor).x) += (_uglobVar);
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._ucolor = ANGLE_output_global._ucolor;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, GLFragColorWithUniform)
{
    const std::string &shaderString =
        R"(
uniform mediump vec4 u_color;
void main(void)
{
    gl_FragColor = u_color;
})";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Output_Global {
  gl_FragColor_ : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(0) gl_FragColor_ : vec4<f32>,
};

@group(0) @binding(1) var<uniform> ANGLE_defaultUniformBlock : ANGLE_DefaultUniformBlock;
@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

struct ANGLE_DefaultUniformBlock
{
  @align(16) _uu_color : vec4<f32>,
};

;

fn _umain()
{
  (ANGLE_output_global.gl_FragColor_) = ((ANGLE_defaultUniformBlock)._uu_color);
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated.gl_FragColor_ = ANGLE_output_global.gl_FragColor_;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, UniformsWithNestedStructs)
{
    const std::string &shaderString =
        R"(#version 300 es
precision mediump float;
struct NestedUniforms {
    float x;
};
struct Uniforms {
    NestedUniforms a;
    float b;
    float c;
    float[5] d;
    float e;
    vec3 f[7];
    float[5] g;
};
uniform Uniforms unis;
out vec4 fragColor;
void main() {
    float[5] dCopy = unis.d;
    fragColor = vec4(unis.a.x, unis.b, unis.c, dCopy[1]);
    fragColor += vec4(unis.d[2], unis.e, unis.f[0][2], (unis.e > 0.5 ? unis.d : unis.g)[1]);
})";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Output_Global {
  _ufragColor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _ufragColor : vec4<f32>,
};

@group(0) @binding(1) var<uniform> ANGLE_defaultUniformBlock : ANGLE_DefaultUniformBlock;
@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLE_wrapped_float
{
  @align(16) elem : f32
};
fn ANGLE_Convert_Array5_ANGLE_wrapped_float_ElementsTo_float_Elements(wrappedArr : array<ANGLE_wrapped_float, 5>) -> array<f32, 5>
{
  var retVal : array<f32, 5>;
  for (var i : u32 = 0; i < 5; i++) {;
    retVal[i] = wrappedArr[i].elem;
  }
  return retVal;
}
struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

struct _uNestedUniforms
{
  @align(16) _ux : f32,
};

struct _uUniforms
{
  @align(16) _ua : _uNestedUniforms,
  @align(16) _ub : f32,
  _uc : f32,
  @align(16) _ud : array<ANGLE_wrapped_float, 5>,
  _ue : f32,
  @align(16) _uf : array<vec3<f32>, 7>,
  @align(16) _ug : array<ANGLE_wrapped_float, 5>,
};

;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;
;

struct ANGLE_DefaultUniformBlock
{
  @align(16) _uunis : _uUniforms,
};

;

fn _umain()
{
  var _udCopy : array<f32, 5> = (ANGLE_Convert_Array5_ANGLE_wrapped_float_ElementsTo_float_Elements(((ANGLE_defaultUniformBlock)._uunis)._ud));
  (ANGLE_output_global._ufragColor) = (vec4<f32>((((ANGLE_defaultUniformBlock)._uunis)._ua)._ux, ((ANGLE_defaultUniformBlock)._uunis)._ub, ((ANGLE_defaultUniformBlock)._uunis)._uc, (_udCopy)[1i]));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(((ANGLE_defaultUniformBlock)._uunis)._ud[2i].elem, ((ANGLE_defaultUniformBlock)._uunis)._ue, ((((ANGLE_defaultUniformBlock)._uunis)._uf)[0i])[2i], (sbc1())[1i]));
}

fn sbc1() -> array<f32, 5>
{
  if ((((ANGLE_defaultUniformBlock)._uunis)._ue) > (0.5f))
  {
    return ANGLE_Convert_Array5_ANGLE_wrapped_float_ElementsTo_float_Elements(((ANGLE_defaultUniformBlock)._uunis)._ud);
  }
  else
  {
    return ANGLE_Convert_Array5_ANGLE_wrapped_float_ElementsTo_float_Elements(((ANGLE_defaultUniformBlock)._uunis)._ug);
  }
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._ufragColor = ANGLE_output_global._ufragColor;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, Ternaries)
{
    const std::string &shaderString =
        R"(#version 300 es
precision mediump float;

float globVar;

in float inVar;
out vec4 fragColor;
void main() {
  fragColor = vec4(0.0);
  // Basic ternary
  fragColor.x = inVar > 0.5 ? 1.0 : 0.0;
  // Ternary with reference to temp vars
  float x = inVar + 1.0;
  fragColor.y = x > 0.5 ? 1.0 + x : x - 1.0;
  // Ternary with reference to global vars or in vars
  globVar = inVar - 2.0;
  fragColor.z = x > globVar ? 1.0 + x : x - 1.0;

  float y = inVar - 7.0;
  fragColor.w = (x > globVar ? (x > globVar + 0.5 ? y + 0.5 : y - 0.5) : y);

  float z = (x > globVar ? y : x);
  fragColor.w += z;

  fragColor.w += (z > 0.5 ? z : z + 0.5);
})";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Input_Global {
  _uinVar : f32,
};

var<private> ANGLE_input_global : ANGLE_Input_Global;

struct ANGLE_Input_Annotated {
  @location(@@@@@@) _uinVar : f32,
};

struct ANGLE_Output_Global {
  _ufragColor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _ufragColor : vec4<f32>,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

var<private> _uglobVar : f32;
;
;
var<private> _ux : f32;
var<private> _uy : f32;
var<private> _uz : f32;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;
;
;
;
;
;
;
;

fn _umain()
{
  (ANGLE_output_global._ufragColor) = (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f));
  ((ANGLE_output_global._ufragColor).x) = (sbc3());
  (_ux) = ((ANGLE_input_global._uinVar) + (1.0f));
  ((ANGLE_output_global._ufragColor).y) = (sbc4());
  (_uglobVar) = ((ANGLE_input_global._uinVar) - (2.0f));
  ((ANGLE_output_global._ufragColor).z) = (sbc5());
  (_uy) = ((ANGLE_input_global._uinVar) - (7.0f));
  ((ANGLE_output_global._ufragColor).w) = (sbc6());
  (_uz) = (sbc7());
  ((ANGLE_output_global._ufragColor).w) += (_uz);
  ((ANGLE_output_global._ufragColor).w) += (sbc8());
}

fn sbc3() -> f32
{
  if ((ANGLE_input_global._uinVar) > (0.5f))
  {
    return 1.0f;
  }
  else
  {
    return 0.0f;
  }
}

fn sbc4() -> f32
{
  if ((_ux) > (0.5f))
  {
    return (1.0f) + (_ux);
  }
  else
  {
    return (_ux) - (1.0f);
  }
}

fn sbc5() -> f32
{
  if ((_ux) > (_uglobVar))
  {
    return (1.0f) + (_ux);
  }
  else
  {
    return (_ux) - (1.0f);
  }
}

fn sbc6() -> f32
{
  if ((_ux) > (_uglobVar))
  {
    return sbc9();
  }
  else
  {
    return _uy;
  }
}

fn sbc7() -> f32
{
  if ((_ux) > (_uglobVar))
  {
    return _uy;
  }
  else
  {
    return _ux;
  }
}

fn sbc8() -> f32
{
  if ((_uz) > (0.5f))
  {
    return _uz;
  }
  else
  {
    return (_uz) + (0.5f);
  }
}

fn sbc9() -> f32
{
  if ((_ux) > ((_uglobVar) + (0.5f)))
  {
    return (_uy) + (0.5f);
  }
  else
  {
    return (_uy) - (0.5f);
  }
}
@fragment
fn wgslMain(ANGLE_input_annotated : ANGLE_Input_Annotated) -> ANGLE_Output_Annotated
{
  ANGLE_input_global._uinVar = ANGLE_input_annotated._uinVar;
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._ufragColor = ANGLE_output_global._ufragColor;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, CommaOperator)
{
    const std::string &shaderString =
        R"(#version 300 es
precision mediump float;

float globVar;

in float inVar;
out vec4 fragColor;

void setGlobVar() {
  globVar = 1.0;
}

void main() {
  fragColor = vec4(0.0);
  float tempVar;
  fragColor.x = (globVar = inVar, tempVar = globVar, tempVar);

  (tempVar = 5.0, globVar = 6.0, setGlobVar());

  float a,b,c,d,e;
  fragColor.x += ((a = 1.0, b = a), (c = b, (d = c)), (setGlobVar(), e = d, e));
})";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Input_Global {
  _uinVar : f32,
};

var<private> ANGLE_input_global : ANGLE_Input_Global;

struct ANGLE_Input_Annotated {
  @location(@@@@@@) _uinVar : f32,
};

struct ANGLE_Output_Global {
  _ufragColor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _ufragColor : vec4<f32>,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

var<private> _uglobVar : f32;
;
;
var<private> _utempVar : f32;
var<private> _ua : f32;
var<private> _ub : f32;
var<private> _uc : f32;
var<private> _ud : f32;
var<private> _ue : f32;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;
;
;
;

fn _usetGlobVar()
{
  (_uglobVar) = (1.0f);
}

fn _umain()
{
  (ANGLE_output_global._ufragColor) = (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f));
  ((ANGLE_output_global._ufragColor).x) = (sbca());
  sbcb();
  ((ANGLE_output_global._ufragColor).x) += (sbcc());
}

fn sbca() -> f32
{
  (_uglobVar) = (ANGLE_input_global._uinVar);
  (_utempVar) = (_uglobVar);
  return _utempVar;
}

fn sbcb()
{
  (_utempVar) = (5.0f);
  (_uglobVar) = (6.0f);
  _usetGlobVar();
}

fn sbcc() -> f32
{
  (_ua) = (1.0f);
  (_ub) = (_ua);
  (_uc) = (_ub);
  (_ud) = (_uc);
  _usetGlobVar();
  (_ue) = (_ud);
  return _ue;
}
@fragment
fn wgslMain(ANGLE_input_annotated : ANGLE_Input_Annotated) -> ANGLE_Output_Annotated
{
  ANGLE_input_global._uinVar = ANGLE_input_annotated._uinVar;
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._ufragColor = ANGLE_output_global._ufragColor;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, DifficultMultiElementSwizzle)
{
    const std::string &shaderString =
        R"(#version 300 es
precision mediump float;

float globVar;

in float inVar;
out vec4 fragColor;
void main() {
  fragColor.xy = vec2(1.0, 2.0);

  vec4[2] vecs;
  int i = 0;
  float a = 0.0;
  vecs[i++].yz = (vecs[i++].xy = vec2(a++, a++));
})";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
fn ANGLE_postIncPriv_1(x : ptr<private, f32>) -> f32 {
  var old = *x;
  (*x) += f32(1);
  return old;
}
fn ANGLE_postIncFunc_1(x : ptr<function, f32>) -> f32 {
  var old = *x;
  (*x) += f32(1);
  return old;
}
fn ANGLE_postIncPriv_0(x : ptr<private, i32>) -> i32 {
  var old = *x;
  (*x) += i32(1);
  return old;
}
fn ANGLE_postIncFunc_0(x : ptr<function, i32>) -> i32 {
  var old = *x;
  (*x) += i32(1);
  return old;
}
struct ANGLE_Output_Global {
  _ufragColor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _ufragColor : vec4<f32>,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

var<private> _uinVar : f32;
;
var<private> _uvecs : array<vec4<f32>, 2>;
var<private> _ui : i32;
var<private> _ua : f32;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;
;
;
;
;

fn _umain()
{
  var sbd9 : vec2<f32> = (vec2<f32>(1.0f, 2.0f));
  ((ANGLE_output_global._ufragColor).x) = ((sbd9).x);
  ((ANGLE_output_global._ufragColor).y) = ((sbd9).y);
  (_ui) = (0i);
  (_ua) = (0.0f);
  sbca();
}

fn sbc4(sbc3 : ptr<function, vec4<f32>>) -> vec2<f32>
{
  var sbda : vec2<f32> = (sbd7(&(*sbc3)));
  (((*sbc3)).y) = ((sbda).x);
  (((*sbc3)).z) = ((sbda).y);
  return ((*sbc3)).yz;
}

fn sbca() -> vec2<f32>
{
  let sbc6 : ptr<private, vec4<f32>> = (&((_uvecs)[clamp((ANGLE_postIncPriv_0(&(_ui))), 0, 1)]));
  var sbc5 : vec4<f32>;
  var sbc9 : vec2<f32> = (sbc4(&sbc5));
  ((*sbc6)) = (sbc5);
  return sbc9;
}

fn sbcc(sbcb : ptr<function, vec4<f32>>, sbc3 : ptr<function, vec4<f32>>) -> vec2<f32>
{
  var sbdb : vec2<f32> = (vec2<f32>(ANGLE_postIncPriv_1(&(_ua)), ANGLE_postIncPriv_1(&(_ua))));
  (((*sbcb)).x) = ((sbdb).x);
  (((*sbcb)).y) = ((sbdb).y);
  return ((*sbcb)).xy;
}

fn sbd7(sbc3 : ptr<function, vec4<f32>>) -> vec2<f32>
{
  let sbcf : ptr<private, vec4<f32>> = (&((_uvecs)[clamp((ANGLE_postIncPriv_0(&(_ui))), 0, 1)]));
  var sbce : vec4<f32>;
  let sbd3 : ptr<function, vec4<f32>> = (&((*sbc3)));
  var sbd2 : vec4<f32>;
  var sbd6 : vec2<f32> = (sbcc(&sbce, &sbd2));
  ((*sbcf)) = (sbce);
  ((*sbd3)) = (sbd2);
  return sbd6;
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._ufragColor = ANGLE_output_global._ufragColor;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, UniformsWithMatCx2)
{
    const std::string &shaderString =
        R"(#version 300 es
precision mediump float;
struct Uniforms {
    mat2 a;
    mat3x2 b;
    mat4x2 c;

    mat2[2] aArr;
    mat3x2[2] bArr;
    mat4x2[2] cArr;
};
uniform Uniforms unis;
out vec4 fragColor;
void main() {
  mat2 a = unis.a;
  mat3x2 b = unis.b;
  mat4x2 c = unis.c;

  mat2[2] aArr = unis.aArr;
  mat3x2[2] bArr = unis.bArr;
  mat4x2[2] cArr = unis.cArr;

  mat2 aIndexed = unis.aArr[1];
  mat3x2 bIndexed = unis.bArr[1];
  mat4x2 cIndexed = unis.cArr[1];

  fragColor = vec4(a[0][0], b[0][0], c[0][0], 1.0);
  fragColor += vec4(aArr[0][0][0], bArr[0][0][0], cArr[0][0][0], 1.0);
  fragColor += vec4(aIndexed[0][0], bIndexed[0][0], cIndexed[0][0], 1.0);
})";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Output_Global {
  _ufragColor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _ufragColor : vec4<f32>,
};

@group(0) @binding(1) var<uniform> ANGLE_defaultUniformBlock : ANGLE_DefaultUniformBlock;
@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLE_wrapped_vec2
{
  @align(16) elem : vec2<f32>
};
fn ANGLE_Convert_Mat2x2(mangledMatrix : array<ANGLE_wrapped_vec2, 2>) -> mat2x2<f32>
{
  var retVal : mat2x2<f32>;
  retVal = mat2x2<f32>(mangledMatrix[0].elem, mangledMatrix[1].elem);
  return retVal;
}
fn ANGLE_Convert_Array2_Mat2x2(mangledMatrix : array<array<ANGLE_wrapped_vec2, 2>, 2>) -> array<mat2x2<f32>, 2>
{
  var retVal : array<mat2x2<f32>, 2>;
  for (var i : u32 = 0; i < 2; i++) {;
    retVal[i] = mat2x2<f32>(mangledMatrix[i][0].elem, mangledMatrix[i][1].elem);
  }
  return retVal;
}
fn ANGLE_Convert_Mat3x2(mangledMatrix : array<ANGLE_wrapped_vec2, 3>) -> mat3x2<f32>
{
  var retVal : mat3x2<f32>;
  retVal = mat3x2<f32>(mangledMatrix[0].elem, mangledMatrix[1].elem, mangledMatrix[2].elem);
  return retVal;
}
fn ANGLE_Convert_Array2_Mat3x2(mangledMatrix : array<array<ANGLE_wrapped_vec2, 3>, 2>) -> array<mat3x2<f32>, 2>
{
  var retVal : array<mat3x2<f32>, 2>;
  for (var i : u32 = 0; i < 2; i++) {;
    retVal[i] = mat3x2<f32>(mangledMatrix[i][0].elem, mangledMatrix[i][1].elem, mangledMatrix[i][2].elem);
  }
  return retVal;
}
fn ANGLE_Convert_Mat4x2(mangledMatrix : array<ANGLE_wrapped_vec2, 4>) -> mat4x2<f32>
{
  var retVal : mat4x2<f32>;
  retVal = mat4x2<f32>(mangledMatrix[0].elem, mangledMatrix[1].elem, mangledMatrix[2].elem, mangledMatrix[3].elem);
  return retVal;
}
fn ANGLE_Convert_Array2_Mat4x2(mangledMatrix : array<array<ANGLE_wrapped_vec2, 4>, 2>) -> array<mat4x2<f32>, 2>
{
  var retVal : array<mat4x2<f32>, 2>;
  for (var i : u32 = 0; i < 2; i++) {;
    retVal[i] = mat4x2<f32>(mangledMatrix[i][0].elem, mangledMatrix[i][1].elem, mangledMatrix[i][2].elem, mangledMatrix[i][3].elem);
  }
  return retVal;
}
struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

struct _uUniforms
{
  @align(16) _ua : array<ANGLE_wrapped_vec2, 2>,
  @align(16) _ub : array<ANGLE_wrapped_vec2, 3>,
  @align(16) _uc : array<ANGLE_wrapped_vec2, 4>,
  @align(16) _uaArr : array<array<ANGLE_wrapped_vec2, 2>, 2>,
  @align(16) _ubArr : array<array<ANGLE_wrapped_vec2, 3>, 2>,
  @align(16) _ucArr : array<array<ANGLE_wrapped_vec2, 4>, 2>,
};

;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

struct ANGLE_DefaultUniformBlock
{
  @align(16) _uunis : _uUniforms,
};

;

fn _umain()
{
  var _ua : mat2x2<f32> = (ANGLE_Convert_Mat2x2(((ANGLE_defaultUniformBlock)._uunis)._ua));
  var _ub : mat3x2<f32> = (ANGLE_Convert_Mat3x2(((ANGLE_defaultUniformBlock)._uunis)._ub));
  var _uc : mat4x2<f32> = (ANGLE_Convert_Mat4x2(((ANGLE_defaultUniformBlock)._uunis)._uc));
  var _uaArr : array<mat2x2<f32>, 2> = (ANGLE_Convert_Array2_Mat2x2(((ANGLE_defaultUniformBlock)._uunis)._uaArr));
  var _ubArr : array<mat3x2<f32>, 2> = (ANGLE_Convert_Array2_Mat3x2(((ANGLE_defaultUniformBlock)._uunis)._ubArr));
  var _ucArr : array<mat4x2<f32>, 2> = (ANGLE_Convert_Array2_Mat4x2(((ANGLE_defaultUniformBlock)._uunis)._ucArr));
  var _uaIndexed : mat2x2<f32> = (ANGLE_Convert_Mat2x2(((ANGLE_defaultUniformBlock)._uunis)._uaArr[1i]));
  var _ubIndexed : mat3x2<f32> = (ANGLE_Convert_Mat3x2(((ANGLE_defaultUniformBlock)._uunis)._ubArr[1i]));
  var _ucIndexed : mat4x2<f32> = (ANGLE_Convert_Mat4x2(((ANGLE_defaultUniformBlock)._uunis)._ucArr[1i]));
  (ANGLE_output_global._ufragColor) = (vec4<f32>(((_ua)[0i])[0i], ((_ub)[0i])[0i], ((_uc)[0i])[0i], 1.0f));
  (ANGLE_output_global._ufragColor) += (vec4<f32>((((_uaArr)[0i])[0i])[0i], (((_ubArr)[0i])[0i])[0i], (((_ucArr)[0i])[0i])[0i], 1.0f));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(((_uaIndexed)[0i])[0i], ((_ubIndexed)[0i])[0i], ((_ucIndexed)[0i])[0i], 1.0f));
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._ufragColor = ANGLE_output_global._ufragColor;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, UniformsWithBool)
{
    const std::string &shaderString =
        R"(#version 300 es
precision mediump float;
struct Uniforms {
    bool a;

    bool[2] aArr;
};
uniform Uniforms unis;
out vec4 fragColor;
void main() {
  bool a = unis.a;

  bool[2] aArr = unis.aArr;

  bool aIndexed = unis.aArr[1];

  fragColor = vec4(a, aArr[0], aIndexed, 1.0);
})";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Output_Global {
  _ufragColor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _ufragColor : vec4<f32>,
};

@group(0) @binding(1) var<uniform> ANGLE_defaultUniformBlock : ANGLE_DefaultUniformBlock;
@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLE_wrapped_uint
{
  @align(16) elem : u32
};
fn ANGLE_Convert_Array2_ANGLE_wrapped_uint_ElementsTo_bool_Elements(wrappedArr : array<ANGLE_wrapped_uint, 2>) -> array<bool, 2>
{
  var retVal : array<bool, 2>;
  for (var i : u32 = 0; i < 2; i++) {;
    retVal[i] = bool(wrappedArr[i].elem);
  }
  return retVal;
}
struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

struct _uUniforms
{
  @align(16) _ua : u32,
  @align(16) _uaArr : array<ANGLE_wrapped_uint, 2>,
};

;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

struct ANGLE_DefaultUniformBlock
{
  @align(16) _uunis : _uUniforms,
};

;

fn _umain()
{
  var _ua : bool = (bool(((ANGLE_defaultUniformBlock)._uunis)._ua));
  var _uaArr : array<bool, 2> = (ANGLE_Convert_Array2_ANGLE_wrapped_uint_ElementsTo_bool_Elements(((ANGLE_defaultUniformBlock)._uunis)._uaArr));
  var _uaIndexed : bool = (bool(((ANGLE_defaultUniformBlock)._uunis)._uaArr[1i].elem));
  (ANGLE_output_global._ufragColor) = (vec4<f32>(_ua, (_uaArr)[0i], _uaIndexed, 1.0f));
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._ufragColor = ANGLE_output_global._ufragColor;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, BasicSamplers)
{
    const std::string &shaderString =
        R"(#version 300 es
precision mediump float;
struct SamplerStruct {
  sampler2D samp1;
};
uniform SamplerStruct sampStruct;

uniform sampler2D samp2D;
uniform mediump sampler3D samp3D;
uniform mediump samplerCube sampCube;

out vec4 fragColor;
void main() {
  fragColor = texture(sampStruct.samp1, vec2(0.0, 0.0));

  // --- texture ---
  fragColor += texture(samp2D, vec2(0.0, 0.0));
  fragColor += texture(samp3D, vec3(0.0, 0.0, 0.0));
  fragColor += texture(sampCube, vec3(0.0, 0.0, 0.0));
  // --- texture with bias ---
  fragColor += texture(samp2D, vec2(0.0, 0.0), /*bias=*/1.0);
  fragColor += texture(samp3D, vec3(0.0, 0.0, 0.0), /*bias=*/1.0);
  fragColor += texture(sampCube, vec3(0.0, 0.0, 0.0), /*bias=*/1.0);

  // --- textureOffset ---
  fragColor += textureOffset(samp2D, vec2(0.0, 0.0), ivec2(5,6));
  fragColor += textureOffset(samp3D, vec3(0.0, 0.0, 0.0), ivec3(5,6,7));
  // --- textureOffset with bias ---
  fragColor += textureOffset(samp2D, vec2(0.0, 0.0), ivec2(5,6), /*bias=*/1.0);
  fragColor += textureOffset(samp3D, vec3(0.0, 0.0, 0.0), ivec3(5,6,7), /*bias=*/1.0);

  // --- textureProj ---
  // All coordinates should be divided by the last
  // --- textureProj with bias---
  fragColor += textureProj(samp2D, vec3(0.0, 0.0, 1.0), /*bias=*/1.0);
  fragColor += textureProj(samp2D, vec4(0.0, 0.0, 0.0, 1.0), /*bias=*/1.0); // 3rd coordinate ignored
  fragColor += textureProj(samp3D, vec4(0.0, 0.0, 0.0, 1.0), /*bias=*/1.0);

  // --- textureLod ---
  fragColor += textureLod(samp2D, vec2(0.5, 0.5), /*lod=*/0.0);
  fragColor += textureLod(samp3D, vec3(0.5, 0.5, 0.5), /*lod=*/0.0);
  fragColor += textureLod(sampCube, vec3(0.5, 0.5, 0.5), /*lod=*/0.0);

  // --- texelFetch ---
  ivec2 size2D = textureSize(samp2D, 0);
  ivec3 size3D = textureSize(samp3D, 0);
  fragColor += texelFetch(samp2D, ivec2(vec2(0.5, 0.5) * vec2(size2D)), 0);
  fragColor += texelFetch(samp3D, ivec3(vec3(0.5, 0.5, 0.5) * vec3(size3D)), 0);

  // --- texelFetchOffset ---
  fragColor += texelFetchOffset(samp2D, ivec2(vec2(0.5, 0.5) * vec2(size2D)), 0, ivec2(0, 0));
  fragColor += texelFetchOffset(samp3D, ivec3(vec3(0.5, 0.5, 0.5) * vec3(size3D)), 0, ivec3(0, 0, 0));

  // --- textureProjOffset ---
  fragColor += textureProjOffset(samp2D, vec3(0.5, 0.5, 1.0), ivec2(0, 0));
  fragColor += textureProjOffset(samp2D, vec4(0.5, 0.5, 0.0, 1.0), ivec2(0, 0));
  fragColor += textureProjOffset(samp3D, vec4(0.5, 0.5, 0.5, 1.0), ivec3(0, 0, 0));
  // --- textureProjOffset with bias ---
  fragColor += textureProjOffset(samp2D, vec3(0.5, 0.5, 1.0), ivec2(0, 0), /*bias=*/1.0);
  fragColor += textureProjOffset(samp2D, vec4(0.5, 0.5, 0.0, 1.0), ivec2(0, 0), /*bias=*/1.0);
  fragColor += textureProjOffset(samp3D, vec4(0.5, 0.5, 0.5, 1.0), ivec3(0, 0, 0), /*bias=*/1.0);

  // --- textureLodOffset ---
  fragColor += textureLodOffset(samp2D, vec2(0.5, 0.5), 0.0, ivec2(0, 0));
  fragColor += textureLodOffset(samp3D, vec3(0.5, 0.5, 0.5), 0.0, ivec3(0, 0, 0));

  // --- textureProjLod ---
  fragColor += textureProjLod(samp2D, vec3(0.5, 0.5, 1.0), /*lod=*/0.0);
  fragColor += textureProjLod(samp2D, vec4(0.5, 0.5, 0.0, 1.0), /*lod=*/0.0);
  fragColor += textureProjLod(samp3D, vec4(0.5, 0.5, 0.5, 1.0), /*lod=*/0.0);

  // --- textureProjLodOffset ---
  fragColor += textureProjLodOffset(samp2D, vec3(0.5, 0.5, 1.0), 0.0, ivec2(0, 0));
  fragColor += textureProjLodOffset(samp2D, vec4(0.5, 0.5, 0.0, 1.0), 0.0, ivec2(0, 0));
  fragColor += textureProjLodOffset(samp3D, vec4(0.5, 0.5, 0.5, 1.0), 0.0, ivec3(0, 0, 0));

  // --- textureGrad ---
  fragColor += textureGrad(samp2D, vec2(0.5, 0.5), vec2(0.0, 0.0), vec2(0.0, 0.0));
  fragColor += textureGrad(samp3D, vec3(0.5, 0.5, 0.5), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0));
  fragColor += textureGrad(sampCube, vec3(0.5, 0.5, 0.5), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0));

  // --- textureGradOffset ---
  fragColor += textureGradOffset(samp2D, vec2(0.5, 0.5), vec2(0.0, 0.0), vec2(0.0, 0.0), ivec2(0, 0));
  fragColor += textureGradOffset(samp3D, vec3(0.5, 0.5, 0.5), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), ivec3(0, 0, 0));

  // --- textureProjGrad ---
  fragColor += textureProjGrad(samp2D, vec3(0.5, 0.5, 1.0), vec2(0.0, 0.0), vec2(0.0, 0.0));
  fragColor += textureProjGrad(samp2D, vec4(0.5, 0.5, 0.0, 1.0), vec2(0.0, 0.0), vec2(0.0, 0.0));
  fragColor += textureProjGrad(samp3D, vec4(0.5, 0.5, 0.5, 1.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0));

  // --- textureProjGradOffset ---
  fragColor += textureProjGradOffset(samp2D, vec3(0.5, 0.5, 1.0), vec2(0.0, 0.0), vec2(0.0, 0.0), ivec2(0, 0));
  fragColor += textureProjGradOffset(samp2D, vec4(0.5, 0.5, 0.0, 1.0), vec2(0.0, 0.0), vec2(0.0, 0.0), ivec2(0, 0));
  fragColor += textureProjGradOffset(samp3D, vec4(0.5, 0.5, 0.5, 1.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), ivec3(0, 0, 0));
}
)";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Output_Global {
  _ufragColor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _ufragColor : vec4<f32>,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;
@group(1) @binding(@@@@@@) var ANGLE_sampler_samp2D : sampler;
@group(1) @binding(@@@@@@) var ANGLE_texture_samp2D : texture_2d<f32>;
@group(1) @binding(@@@@@@) var ANGLE_sampler_samp3D : sampler;
@group(1) @binding(@@@@@@) var ANGLE_texture_samp3D : texture_3d<f32>;
@group(1) @binding(@@@@@@) var ANGLE_sampler_sampCube : sampler;
@group(1) @binding(@@@@@@) var ANGLE_texture_sampCube : texture_cube<f32>;
@group(1) @binding(@@@@@@) var ANGLE_sampler_sampStruct_samp1 : sampler;
@group(1) @binding(@@@@@@) var ANGLE_texture_sampStruct_samp1 : texture_2d<f32>;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

;
;
;
;
;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

fn _umain()
{
  (ANGLE_output_global._ufragColor) = (textureSample(ANGLE_texture_sampStruct_samp1, ANGLE_sampler_sampStruct_samp1, (vec2<f32>(0.0f, 0.0f)).xy));
  (ANGLE_output_global._ufragColor) += (textureSample(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec2<f32>(0.0f, 0.0f)).xy));
  (ANGLE_output_global._ufragColor) += (textureSample(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec3<f32>(0.0f, 0.0f, 0.0f)).xyz));
  (ANGLE_output_global._ufragColor) += (textureSample(ANGLE_texture_sampCube, ANGLE_sampler_sampCube, (vec3<f32>(0.0f, 0.0f, 0.0f)).xyz));
  (ANGLE_output_global._ufragColor) += (textureSampleBias(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec2<f32>(0.0f, 0.0f)).xy, 1.0f));
  (ANGLE_output_global._ufragColor) += (textureSampleBias(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec3<f32>(0.0f, 0.0f, 0.0f)).xyz, 1.0f));
  (ANGLE_output_global._ufragColor) += (textureSampleBias(ANGLE_texture_sampCube, ANGLE_sampler_sampCube, (vec3<f32>(0.0f, 0.0f, 0.0f)).xyz, 1.0f));
  (ANGLE_output_global._ufragColor) += (textureSample(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec2<f32>(0.0f, 0.0f)).xy, vec2<i32>(5i, 6i)));
  (ANGLE_output_global._ufragColor) += (textureSample(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec3<f32>(0.0f, 0.0f, 0.0f)).xyz, vec3<i32>(5i, 6i, 7i)));
  (ANGLE_output_global._ufragColor) += (textureSampleBias(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec2<f32>(0.0f, 0.0f)).xy, 1.0f, vec2<i32>(5i, 6i)));
  (ANGLE_output_global._ufragColor) += (textureSampleBias(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec3<f32>(0.0f, 0.0f, 0.0f)).xyz, 1.0f, vec3<i32>(5i, 6i, 7i)));
  (ANGLE_output_global._ufragColor) += (textureSampleBias(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec3<f32>(0.0f, 0.0f, 1.0f)).xy / (vec3<f32>(0.0f, 0.0f, 1.0f)).z, 1.0f));
  (ANGLE_output_global._ufragColor) += (textureSampleBias(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec4<f32>(0.0f, 0.0f, 0.0f, 1.0f)).xy / (vec4<f32>(0.0f, 0.0f, 0.0f, 1.0f)).w, 1.0f));
  (ANGLE_output_global._ufragColor) += (textureSampleBias(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec4<f32>(0.0f, 0.0f, 0.0f, 1.0f)).xyz / (vec4<f32>(0.0f, 0.0f, 0.0f, 1.0f)).w, 1.0f));
  (ANGLE_output_global._ufragColor) += (textureSampleLevel(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec2<f32>(0.5f, 0.5f)).xy, 0.0f));
  (ANGLE_output_global._ufragColor) += (textureSampleLevel(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec3<f32>(0.5f, 0.5f, 0.5f)).xyz, 0.0f));
  (ANGLE_output_global._ufragColor) += (textureSampleLevel(ANGLE_texture_sampCube, ANGLE_sampler_sampCube, (vec3<f32>(0.5f, 0.5f, 0.5f)).xyz, 0.0f));
  var _usize2D : vec2<i32> = (textureDimensions(ANGLE_texture_samp2D, 0i));
  var _usize3D : vec3<i32> = (textureDimensions(ANGLE_texture_samp3D, 0i));
  (ANGLE_output_global._ufragColor) += (textureLoad(ANGLE_texture_samp2D, (vec2<i32>((vec2<f32>(0.5f, 0.5f)) * (vec2<f32>(_usize2D)))), 0i));
  (ANGLE_output_global._ufragColor) += (textureLoad(ANGLE_texture_samp3D, (vec3<i32>((vec3<f32>(0.5f, 0.5f, 0.5f)) * (vec3<f32>(_usize3D)))), 0i));
  (ANGLE_output_global._ufragColor) += (textureLoad(ANGLE_texture_samp2D, (vec2<i32>((vec2<f32>(0.5f, 0.5f)) * (vec2<f32>(_usize2D)))), 0i, vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureLoad(ANGLE_texture_samp3D, (vec3<i32>((vec3<f32>(0.5f, 0.5f, 0.5f)) * (vec3<f32>(_usize3D)))), 0i, vec3<i32>(0i, 0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSample(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec3<f32>(0.5f, 0.5f, 1.0f)).xy / (vec3<f32>(0.5f, 0.5f, 1.0f)).z, vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSample(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec4<f32>(0.5f, 0.5f, 0.0f, 1.0f)).xy / (vec4<f32>(0.5f, 0.5f, 0.0f, 1.0f)).w, vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSample(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).xyz / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, vec3<i32>(0i, 0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleBias(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec3<f32>(0.5f, 0.5f, 1.0f)).xy / (vec3<f32>(0.5f, 0.5f, 1.0f)).z, 1.0f, vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleBias(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec4<f32>(0.5f, 0.5f, 0.0f, 1.0f)).xy / (vec4<f32>(0.5f, 0.5f, 0.0f, 1.0f)).w, 1.0f, vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleBias(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).xyz / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, 1.0f, vec3<i32>(0i, 0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleLevel(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec2<f32>(0.5f, 0.5f)).xy, 0.0f, vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleLevel(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec3<f32>(0.5f, 0.5f, 0.5f)).xyz, 0.0f, vec3<i32>(0i, 0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleLevel(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec3<f32>(0.5f, 0.5f, 1.0f)).xy / (vec3<f32>(0.5f, 0.5f, 1.0f)).z, 0.0f));
  (ANGLE_output_global._ufragColor) += (textureSampleLevel(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec4<f32>(0.5f, 0.5f, 0.0f, 1.0f)).xy / (vec4<f32>(0.5f, 0.5f, 0.0f, 1.0f)).w, 0.0f));
  (ANGLE_output_global._ufragColor) += (textureSampleLevel(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).xyz / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, 0.0f));
  (ANGLE_output_global._ufragColor) += (textureSampleLevel(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec3<f32>(0.5f, 0.5f, 1.0f)).xy / (vec3<f32>(0.5f, 0.5f, 1.0f)).z, 0.0f, vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleLevel(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec4<f32>(0.5f, 0.5f, 0.0f, 1.0f)).xy / (vec4<f32>(0.5f, 0.5f, 0.0f, 1.0f)).w, 0.0f, vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleLevel(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).xyz / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, 0.0f, vec3<i32>(0i, 0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleGrad(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec2<f32>(0.5f, 0.5f)).xy, vec2<f32>(0.0f, 0.0f), vec2<f32>(0.0f, 0.0f)));
  (ANGLE_output_global._ufragColor) += (textureSampleGrad(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec3<f32>(0.5f, 0.5f, 0.5f)).xyz, vec3<f32>(0.0f, 0.0f, 0.0f), vec3<f32>(0.0f, 0.0f, 0.0f)));
  (ANGLE_output_global._ufragColor) += (textureSampleGrad(ANGLE_texture_sampCube, ANGLE_sampler_sampCube, (vec3<f32>(0.5f, 0.5f, 0.5f)).xyz, vec3<f32>(0.0f, 0.0f, 0.0f), vec3<f32>(0.0f, 0.0f, 0.0f)));
  (ANGLE_output_global._ufragColor) += (textureSampleGrad(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec2<f32>(0.5f, 0.5f)).xy, vec2<f32>(0.0f, 0.0f), vec2<f32>(0.0f, 0.0f), vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleGrad(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec3<f32>(0.5f, 0.5f, 0.5f)).xyz, vec3<f32>(0.0f, 0.0f, 0.0f), vec3<f32>(0.0f, 0.0f, 0.0f), vec3<i32>(0i, 0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleGrad(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec3<f32>(0.5f, 0.5f, 1.0f)).xy / (vec3<f32>(0.5f, 0.5f, 1.0f)).z, vec2<f32>(0.0f, 0.0f), vec2<f32>(0.0f, 0.0f)));
  (ANGLE_output_global._ufragColor) += (textureSampleGrad(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec4<f32>(0.5f, 0.5f, 0.0f, 1.0f)).xy / (vec4<f32>(0.5f, 0.5f, 0.0f, 1.0f)).w, vec2<f32>(0.0f, 0.0f), vec2<f32>(0.0f, 0.0f)));
  (ANGLE_output_global._ufragColor) += (textureSampleGrad(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).xyz / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, vec3<f32>(0.0f, 0.0f, 0.0f), vec3<f32>(0.0f, 0.0f, 0.0f)));
  (ANGLE_output_global._ufragColor) += (textureSampleGrad(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec3<f32>(0.5f, 0.5f, 1.0f)).xy / (vec3<f32>(0.5f, 0.5f, 1.0f)).z, vec2<f32>(0.0f, 0.0f), vec2<f32>(0.0f, 0.0f), vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleGrad(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec4<f32>(0.5f, 0.5f, 0.0f, 1.0f)).xy / (vec4<f32>(0.5f, 0.5f, 0.0f, 1.0f)).w, vec2<f32>(0.0f, 0.0f), vec2<f32>(0.0f, 0.0f), vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleGrad(ANGLE_texture_samp3D, ANGLE_sampler_samp3D, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).xyz / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, vec3<f32>(0.0f, 0.0f, 0.0f), vec3<f32>(0.0f, 0.0f, 0.0f), vec3<i32>(0i, 0i, 0i)));
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._ufragColor = ANGLE_output_global._ufragColor;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, ShadowSamplers)
{
    const std::string &shaderString =
        R"(#version 300 es
precision mediump float;

uniform mediump sampler2DShadow samp2DShadow;
uniform mediump samplerCubeShadow sampCubeShadow;

out vec4 fragColor;
void main() {
    fragColor = vec4(0.0);

    // --- textureSize ---
    fragColor += vec4(textureSize(samp2DShadow, 0), 0.0, 0.0);
    fragColor += vec4(textureSize(sampCubeShadow, 0), 0.0, 0.0);

    // --- texture ---
    fragColor += vec4(texture(samp2DShadow, vec3(0.5, 0.5, 0.5)));
    fragColor += vec4(texture(sampCubeShadow, vec4(0.5, 0.5, 0.5, 0.5)));
    // --- texture with bias ---
    fragColor += vec4(texture(samp2DShadow, vec3(0.5, 0.5, 0.5), /*bias=*/0.0));
    fragColor += vec4(texture(sampCubeShadow, vec4(0.5, 0.5, 0.5, 0.5), /*bias=*/0.0));

    // --- textureProj ---
    fragColor += vec4(textureProj(samp2DShadow, vec4(0.5, 0.5, 0.5, 1.0)));
    // --- textureProj with bias ---
    fragColor += vec4(textureProj(samp2DShadow, vec4(0.5, 0.5, 0.5, 1.0), /*bias=*/0.0));

    // --- textureLod ---
    fragColor += vec4(textureLod(samp2DShadow, vec3(0.5, 0.5, 0.5), /*lod=*/0.0));

    // --- textureOffset ---
    fragColor += vec4(textureOffset(samp2DShadow, vec3(0.5, 0.5, 0.5), ivec2(0, 0)));
    // --- textureOffset with bias ---
    fragColor += vec4(textureOffset(samp2DShadow, vec3(0.5, 0.5, 0.5), ivec2(0, 0), /*bias=*/0.0));

    // --- textureProjOffset ---
    fragColor += vec4(textureProjOffset(samp2DShadow, vec4(0.5, 0.5, 0.5, 1.0), ivec2(0, 0)));
    // --- textureProjOffset bias ---
    fragColor += vec4(textureProjOffset(samp2DShadow, vec4(0.5, 0.5, 0.5, 1.0), ivec2(0, 0), /*bias=*/0.0));

    // --- textureLodOffset ---
    fragColor += vec4(textureLodOffset(samp2DShadow, vec3(0.5, 0.5, 0.5), 0.0, ivec2(0, 0)));

    // --- textureProjLod ---
    fragColor += vec4(textureProjLod(samp2DShadow, vec4(0.5, 0.5, 0.5, 1.0), /*lod=*/0.0));

    // --- textureProjLodOffset ---
    fragColor += vec4(textureProjLodOffset(samp2DShadow, vec4(0.5, 0.5, 0.5, 1.0), 0.0, ivec2(0, 0)));

    // --- textureGrad ---
    fragColor += vec4(textureGrad(samp2DShadow, vec3(0.5, 0.5, 0.5), vec2(0.0, 0.0), vec2(0.0, 0.0)));
    fragColor += vec4(textureGrad(sampCubeShadow, vec4(0.5, 0.5, 0.5, 0.5), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0)));

    // --- textureGradOffset ---
    fragColor += vec4(textureGradOffset(samp2DShadow, vec3(0.5, 0.5, 0.5), vec2(0.0, 0.0), vec2(0.0, 0.0), ivec2(0, 0)));

    // --- textureProjGrad ---
    fragColor += vec4(textureProjGrad(samp2DShadow, vec4(0.5, 0.5, 0.5, 1.0), vec2(0.0, 0.0), vec2(0.0, 0.0)));

    // --- textureProjGradOffset ---
    fragColor += vec4(textureProjGradOffset(samp2DShadow, vec4(0.5, 0.5, 0.5, 1.0), vec2(0.0, 0.0), vec2(0.0, 0.0), ivec2(0, 0)));
}
)";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Output_Global {
  _ufragColor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _ufragColor : vec4<f32>,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;
@group(1) @binding(@@@@@@) var ANGLE_sampler_samp2DShadow : sampler_comparison;
@group(1) @binding(@@@@@@) var ANGLE_texture_samp2DShadow : texture_depth_2d;
@group(1) @binding(@@@@@@) var ANGLE_sampler_sampCubeShadow : sampler_comparison;
@group(1) @binding(@@@@@@) var ANGLE_texture_sampCubeShadow : texture_depth_cube;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

;
;
;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

fn _umain()
{
  (ANGLE_output_global._ufragColor) = (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(textureDimensions(ANGLE_texture_samp2DShadow, 0i), 0.0f, 0.0f));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(textureDimensions(ANGLE_texture_sampCubeShadow, 0i), 0.0f, 0.0f));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(textureSampleCompare(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, (vec3<f32>(0.5f, 0.5f, 0.5f)).z)));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(textureSampleCompare(ANGLE_texture_sampCubeShadow, ANGLE_sampler_sampCubeShadow, (vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).xyz, (vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).w)));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_BIAS_WITH_SHADOW_SAMPLER(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, (vec3<f32>(0.5f, 0.5f, 0.5f)).z, 0.0f)));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_BIAS_WITH_SHADOW_SAMPLER(ANGLE_texture_sampCubeShadow, ANGLE_sampler_sampCubeShadow, (vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).xyz, (vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).w, 0.0f)));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(textureSampleCompare(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).xy / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).z / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w)));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_BIAS_WITH_SHADOW_SAMPLER(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).xy / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).z / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, 0.0f)));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_EXPLICIT_LOD_WITH_SHADOW_SAMPLER(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, (vec3<f32>(0.5f, 0.5f, 0.5f)).z, 0.0f)));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(textureSampleCompare(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, (vec3<f32>(0.5f, 0.5f, 0.5f)).z, vec2<i32>(0i, 0i))));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_BIAS_WITH_SHADOW_SAMPLER(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, (vec3<f32>(0.5f, 0.5f, 0.5f)).z, 0.0f, vec2<i32>(0i, 0i))));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(textureSampleCompare(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).xy / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).z / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, vec2<i32>(0i, 0i))));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_BIAS_WITH_SHADOW_SAMPLER(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).xy / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).z / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, 0.0f, vec2<i32>(0i, 0i))));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_EXPLICIT_LOD_WITH_SHADOW_SAMPLER(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, (vec3<f32>(0.5f, 0.5f, 0.5f)).z, 0.0f, vec2<i32>(0i, 0i))));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_EXPLICIT_LOD_WITH_SHADOW_SAMPLER(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).xy / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).z / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, 0.0f)));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_EXPLICIT_LOD_WITH_SHADOW_SAMPLER(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).xy / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).z / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, 0.0f, vec2<i32>(0i, 0i))));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_EXPLICIT_GRAD_WITH_SHADOW_SAMPLER(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, (vec3<f32>(0.5f, 0.5f, 0.5f)).z, vec2<f32>(0.0f, 0.0f), vec2<f32>(0.0f, 0.0f))));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_EXPLICIT_GRAD_WITH_SHADOW_SAMPLER(ANGLE_texture_sampCubeShadow, ANGLE_sampler_sampCubeShadow, (vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).xyz, (vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).w, vec3<f32>(0.0f, 0.0f, 0.0f), vec3<f32>(0.0f, 0.0f, 0.0f))));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_EXPLICIT_GRAD_WITH_SHADOW_SAMPLER(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, (vec3<f32>(0.5f, 0.5f, 0.5f)).z, vec2<f32>(0.0f, 0.0f), vec2<f32>(0.0f, 0.0f), vec2<i32>(0i, 0i))));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_EXPLICIT_GRAD_WITH_SHADOW_SAMPLER(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).xy / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).z / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, vec2<f32>(0.0f, 0.0f), vec2<f32>(0.0f, 0.0f))));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_EXPLICIT_GRAD_WITH_SHADOW_SAMPLER(ANGLE_texture_samp2DShadow, ANGLE_sampler_samp2DShadow, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).xy / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).z / (vec4<f32>(0.5f, 0.5f, 0.5f, 1.0f)).w, vec2<f32>(0.0f, 0.0f), vec2<f32>(0.0f, 0.0f), vec2<i32>(0i, 0i))));
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._ufragColor = ANGLE_output_global._ufragColor;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

// Including arrays of shadow samplers.
TEST_F(WGSLOutputTest, ArraySamplers)
{
    const std::string &shaderString =
        R"(#version 300 es
precision mediump float;

uniform mediump sampler2DArray samp2DArray;
uniform mediump sampler2DArrayShadow samp2DArrayShadow;

out vec4 fragColor;
void main() {
  fragColor = vec4(0.0);

  // --- textureSize ---
  fragColor += vec4(textureSize(samp2DArray, 0), 0.0);
  fragColor += vec4(textureSize(samp2DArrayShadow, 0), 0.0);

  // --- texture ---
  fragColor += texture(samp2DArray, vec3(0.5, 0.5, 0.5));
  fragColor += texture(samp2DArray, vec3(0.5, 0.5, 0.5), /*bias=*/0.0);
  fragColor += vec4(texture(samp2DArrayShadow, vec4(0.5, 0.5, 0.5, 0.5)));

  // --- textureLod ---
  fragColor += textureLod(samp2DArray, vec3(0.5, 0.5, 0.5), 0.0);

  // --- textureOffset ---
  fragColor += textureOffset(samp2DArray, vec3(0.5, 0.5, 0.5), ivec2(0, 0));
  fragColor += textureOffset(samp2DArray, vec3(0.5, 0.5, 0.5), ivec2(0, 0), /*bias=*/0.0);

  // --- texelFetch ---
  ivec3 size2DArray = textureSize(samp2DArray, 0);
  fragColor += texelFetch(samp2DArray, ivec3(vec3(0.5, 0.5, 0.5) * vec3(size2DArray)), /*lod=*/0);

  // --- texelFetchOffset ---
  fragColor += texelFetchOffset(samp2DArray, ivec3(vec3(0.5, 0.5, 0.5) * vec3(size2DArray)), 0, ivec2(0, 0));

  // --- textureLodOffset ---
  fragColor += textureLodOffset(samp2DArray, vec3(0.5, 0.5, 0.5), 0.0, ivec2(0, 0));

  // --- textureGrad ---
  fragColor += textureGrad(samp2DArray, vec3(0.5, 0.5, 0.5), vec2(0.0, 0.0), vec2(0.0, 0.0));
  fragColor += vec4(textureGrad(samp2DArrayShadow, vec4(0.5, 0.5, 0.5, 0.5), vec2(0.0, 0.0), vec2(0.0, 0.0)));

  // --- textureGradOffset ---
  fragColor += textureGradOffset(samp2DArray, vec3(0.5, 0.5, 0.5), vec2(0.0, 0.0), vec2(0.0, 0.0), ivec2(0, 0));
  fragColor += textureGradOffset(samp2DArrayShadow, vec4(0.5, 0.5, 0.5, 0.5), vec2(0.0, 0.0), vec2(0.0, 0.0), ivec2(0, 0));
}
)";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Output_Global {
  _ufragColor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _ufragColor : vec4<f32>,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;
@group(1) @binding(@@@@@@) var ANGLE_sampler_samp2DArray : sampler;
@group(1) @binding(@@@@@@) var ANGLE_texture_samp2DArray : texture_2d_array<f32>;
@group(1) @binding(@@@@@@) var ANGLE_sampler_samp2DArrayShadow : sampler_comparison;
@group(1) @binding(@@@@@@) var ANGLE_texture_samp2DArrayShadow : texture_depth_2d_array;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

;
;
;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

fn _umain()
{
  (ANGLE_output_global._ufragColor) = (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(textureDimensions(ANGLE_texture_samp2DArray, 0i), 0.0f));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(textureDimensions(ANGLE_texture_samp2DArrayShadow, 0i), 0.0f));
  (ANGLE_output_global._ufragColor) += (textureSample(ANGLE_texture_samp2DArray, ANGLE_sampler_samp2DArray, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, i32((vec3<f32>(0.5f, 0.5f, 0.5f)).z)));
  (ANGLE_output_global._ufragColor) += (textureSampleBias(ANGLE_texture_samp2DArray, ANGLE_sampler_samp2DArray, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, i32((vec3<f32>(0.5f, 0.5f, 0.5f)).z), 0.0f));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(textureSampleCompare(ANGLE_texture_samp2DArrayShadow, ANGLE_sampler_samp2DArrayShadow, (vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).xy, i32((vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).z), (vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).w)));
  (ANGLE_output_global._ufragColor) += (textureSampleLevel(ANGLE_texture_samp2DArray, ANGLE_sampler_samp2DArray, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, i32((vec3<f32>(0.5f, 0.5f, 0.5f)).z), 0.0f));
  (ANGLE_output_global._ufragColor) += (textureSample(ANGLE_texture_samp2DArray, ANGLE_sampler_samp2DArray, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, i32((vec3<f32>(0.5f, 0.5f, 0.5f)).z), vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleBias(ANGLE_texture_samp2DArray, ANGLE_sampler_samp2DArray, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, i32((vec3<f32>(0.5f, 0.5f, 0.5f)).z), 0.0f, vec2<i32>(0i, 0i)));
  var _usize2DArray : vec3<i32> = (textureDimensions(ANGLE_texture_samp2DArray, 0i));
  (ANGLE_output_global._ufragColor) += (textureLoad(ANGLE_texture_samp2DArray, (vec3<i32>((vec3<f32>(0.5f, 0.5f, 0.5f)) * (vec3<f32>(_usize2DArray)))), 0i));
  (ANGLE_output_global._ufragColor) += (textureLoad(ANGLE_texture_samp2DArray, (vec3<i32>((vec3<f32>(0.5f, 0.5f, 0.5f)) * (vec3<f32>(_usize2DArray)))), 0i, vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleLevel(ANGLE_texture_samp2DArray, ANGLE_sampler_samp2DArray, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, i32((vec3<f32>(0.5f, 0.5f, 0.5f)).z), 0.0f, vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (textureSampleGrad(ANGLE_texture_samp2DArray, ANGLE_sampler_samp2DArray, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, i32((vec3<f32>(0.5f, 0.5f, 0.5f)).z), vec2<f32>(0.0f, 0.0f), vec2<f32>(0.0f, 0.0f)));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(TODO_CANNOT_USE_EXPLICIT_GRAD_WITH_SHADOW_SAMPLER(ANGLE_texture_samp2DArrayShadow, ANGLE_sampler_samp2DArrayShadow, (vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).xy, i32((vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).z), (vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).w, vec2<f32>(0.0f, 0.0f), vec2<f32>(0.0f, 0.0f))));
  (ANGLE_output_global._ufragColor) += (textureSampleGrad(ANGLE_texture_samp2DArray, ANGLE_sampler_samp2DArray, (vec3<f32>(0.5f, 0.5f, 0.5f)).xy, i32((vec3<f32>(0.5f, 0.5f, 0.5f)).z), vec2<f32>(0.0f, 0.0f), vec2<f32>(0.0f, 0.0f), vec2<i32>(0i, 0i)));
  (ANGLE_output_global._ufragColor) += (TODO_CANNOT_USE_EXPLICIT_GRAD_WITH_SHADOW_SAMPLER(ANGLE_texture_samp2DArrayShadow, ANGLE_sampler_samp2DArrayShadow, (vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).xy, i32((vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).z), (vec4<f32>(0.5f, 0.5f, 0.5f, 0.5f)).w, vec2<f32>(0.0f, 0.0f), vec2<f32>(0.0f, 0.0f), vec2<i32>(0i, 0i)));
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._ufragColor = ANGLE_output_global._ufragColor;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

// ES2 versions.
TEST_F(WGSLOutputTest, SamplersES2)
{
    const std::string &shaderString =
        R"(precision mediump float;

uniform sampler2D samp2D;
uniform samplerCube sampCube;

void main() {
  gl_FragColor = vec4(0.0);

  // --- texture2D ---
  gl_FragColor += texture2D(samp2D, vec2(0.0, 0.0));
  gl_FragColor += texture2D(samp2D, vec2(0.0, 0.0), /*bias=*/0.0);

  // --- texture2DProj ---
  gl_FragColor += texture2DProj(samp2D, vec4(0.0, 0.0, 0.0, 0.0).xyz);
  gl_FragColor += texture2DProj(samp2D, vec4(0.0, 0.0, 0.0, 0.0));
  gl_FragColor += texture2DProj(samp2D, vec4(0.0, 0.0, 0.0, 0.0).xyz, /*bias=*/0.0);
  gl_FragColor += texture2DProj(samp2D, vec4(0.0, 0.0, 0.0, 0.0), /*bias=*/0.0);

  // --- textureCube ---
  gl_FragColor += textureCube(sampCube, vec3(0.0, 0.0, 0.0));
  gl_FragColor += textureCube(sampCube, vec3(0.0, 0.0, 0.0), /*bias=*/0.0);

  // Explicit LOD versions not available in fragment shaders.
}
)";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Output_Global {
  gl_FragColor_ : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(0) gl_FragColor_ : vec4<f32>,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;
@group(1) @binding(@@@@@@) var ANGLE_sampler_samp2D : sampler;
@group(1) @binding(@@@@@@) var ANGLE_texture_samp2D : texture_2d<f32>;
@group(1) @binding(@@@@@@) var ANGLE_sampler_sampCube : sampler;
@group(1) @binding(@@@@@@) var ANGLE_texture_sampCube : texture_cube<f32>;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

;
;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

fn _umain()
{
  (ANGLE_output_global.gl_FragColor_) = (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f));
  (ANGLE_output_global.gl_FragColor_) += (textureSample(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec2<f32>(0.0f, 0.0f)).xy));
  (ANGLE_output_global.gl_FragColor_) += (textureSampleBias(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec2<f32>(0.0f, 0.0f)).xy, 0.0f));
  (ANGLE_output_global.gl_FragColor_) += (textureSample(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec3<f32>(0.0f, 0.0f, 0.0f)).xy / (vec3<f32>(0.0f, 0.0f, 0.0f)).z));
  (ANGLE_output_global.gl_FragColor_) += (textureSample(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f)).xy / (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f)).w));
  (ANGLE_output_global.gl_FragColor_) += (textureSampleBias(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec3<f32>(0.0f, 0.0f, 0.0f)).xy / (vec3<f32>(0.0f, 0.0f, 0.0f)).z, 0.0f));
  (ANGLE_output_global.gl_FragColor_) += (textureSampleBias(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f)).xy / (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f)).w, 0.0f));
  (ANGLE_output_global.gl_FragColor_) += (textureSample(ANGLE_texture_sampCube, ANGLE_sampler_sampCube, (vec3<f32>(0.0f, 0.0f, 0.0f)).xyz));
  (ANGLE_output_global.gl_FragColor_) += (textureSampleBias(ANGLE_texture_sampCube, ANGLE_sampler_sampCube, (vec3<f32>(0.0f, 0.0f, 0.0f)).xyz, 0.0f));
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated.gl_FragColor_ = ANGLE_output_global.gl_FragColor_;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLVertexOutputTest, SamplersES2)
{
    const std::string &shaderString =
        R"(precision mediump float;

uniform sampler2D samp2D;
uniform samplerCube sampCube;

void main() {
  gl_Position = vec4(0.0);

  // Bias versions not available in the vertex shader.

  // --- texture2D ---
  gl_Position += texture2D(samp2D, vec2(0.0, 0.0));

  // --- texture2DProj ---
  gl_Position += texture2DProj(samp2D, vec4(0.0, 0.0, 0.0, 0.0).xyz);
  gl_Position += texture2DProj(samp2D, vec4(0.0, 0.0, 0.0, 0.0));

  // --- textureCube ---
  gl_Position += textureCube(sampCube, vec3(0.0, 0.0, 0.0));

  // These explicit LOD versions only available in vertex shaders.

  // --- texture2DLod ---
  gl_Position += texture2DLod(samp2D, vec2(0.0, 0.0), /*lod=*/0.0);

  // --- texture2DProjLod ---
  gl_Position += texture2DProjLod(samp2D, vec4(0.0, 0.0, 0.0, 0.0).xyz, /*lod=*/0.0);
  gl_Position += texture2DProjLod(samp2D, vec4(0.0, 0.0, 0.0, 0.0), /*lod=*/0.0);

  // --- textureCubeLod ---
  gl_Position += textureCubeLod(sampCube, vec3(0.0, 0.0, 0.0), /*lod=*/0.0);
}
)";

    // TODO(anglebug.com/389145696): these are incorrect translations in vertex shaders, They should
    // be textureLoad(), as the basic textureSample*() functions aren't available in WGSL vertex
    // shaders.
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Output_Global {
  gl_Position_ : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @builtin(position) gl_Position_ : vec4<f32>,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;
@group(1) @binding(@@@@@@) var ANGLE_sampler_samp2D : sampler;
@group(1) @binding(@@@@@@) var ANGLE_texture_samp2D : texture_2d<f32>;
@group(1) @binding(@@@@@@) var ANGLE_sampler_sampCube : sampler;
@group(1) @binding(@@@@@@) var ANGLE_texture_sampCube : texture_cube<f32>;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

;
;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

fn _umain()
{
  (ANGLE_output_global.gl_Position_) = (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f));
  (ANGLE_output_global.gl_Position_) += (textureSample(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec2<f32>(0.0f, 0.0f)).xy));
  (ANGLE_output_global.gl_Position_) += (textureSample(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec3<f32>(0.0f, 0.0f, 0.0f)).xy / (vec3<f32>(0.0f, 0.0f, 0.0f)).z));
  (ANGLE_output_global.gl_Position_) += (textureSample(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f)).xy / (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f)).w));
  (ANGLE_output_global.gl_Position_) += (textureSample(ANGLE_texture_sampCube, ANGLE_sampler_sampCube, (vec3<f32>(0.0f, 0.0f, 0.0f)).xyz));
  (ANGLE_output_global.gl_Position_) += (textureSampleLevel(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec2<f32>(0.0f, 0.0f)).xy, 0.0f));
  (ANGLE_output_global.gl_Position_) += (textureSampleLevel(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec3<f32>(0.0f, 0.0f, 0.0f)).xy / (vec3<f32>(0.0f, 0.0f, 0.0f)).z, 0.0f));
  (ANGLE_output_global.gl_Position_) += (textureSampleLevel(ANGLE_texture_samp2D, ANGLE_sampler_samp2D, (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f)).xy / (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f)).w, 0.0f));
  (ANGLE_output_global.gl_Position_) += (textureSampleLevel(ANGLE_texture_sampCube, ANGLE_sampler_sampCube, (vec3<f32>(0.0f, 0.0f, 0.0f)).xyz, 0.0f));
  ((ANGLE_output_global.gl_Position_).y) = (((ANGLE_output_global.gl_Position_).y) * ((unpack4x8snorm((ANGLEUniforms).flipXY)).w));
}
@vertex
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated.gl_Position_ = ANGLE_output_global.gl_Position_;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLVertexOutputTest, MatrixAttributesAndVaryings)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        in mat3 inMat;
        out mat3 outMatArr;

        void main()
        {
          outMatArr = inMat;
        })";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Input_Global {
  _uinMat : mat3x3<f32>,
};

var<private> ANGLE_input_global : ANGLE_Input_Global;

struct ANGLE_Input_Annotated {
  @location(@@@@@@) _uinMat_col0 : vec3<f32>,
  @location(@@@@@@) _uinMat_col1 : vec3<f32>,
  @location(@@@@@@) _uinMat_col2 : vec3<f32>,
};

struct ANGLE_Output_Global {
  gl_Position_ : vec4<f32>,
  _uoutMatArr : mat3x3<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @builtin(position) gl_Position_ : vec4<f32>,
  @location(@@@@@@) _uoutMatArr_col0 : vec3<f32>,
  @location(@@@@@@) _uoutMatArr_col1 : vec3<f32>,
  @location(@@@@@@) _uoutMatArr_col2 : vec3<f32>,
};

@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

;
;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

fn _umain()
{
  (ANGLE_output_global._uoutMatArr) = (ANGLE_input_global._uinMat);
  ((ANGLE_output_global.gl_Position_).y) = (((ANGLE_output_global.gl_Position_).y) * ((unpack4x8snorm((ANGLEUniforms).flipXY)).w));
}
@vertex
fn wgslMain(ANGLE_input_annotated : ANGLE_Input_Annotated) -> ANGLE_Output_Annotated
{
  ANGLE_input_global._uinMat = mat3x3<f32>(ANGLE_input_annotated._uinMat_col0, ANGLE_input_annotated._uinMat_col1, ANGLE_input_annotated._uinMat_col2);
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated.gl_Position_ = ANGLE_output_global.gl_Position_;
  ANGLE_output_annotated._uoutMatArr_col0 = ANGLE_output_global._uoutMatArr[0];
  ANGLE_output_annotated._uoutMatArr_col1 = ANGLE_output_global._uoutMatArr[1];
  ANGLE_output_annotated._uoutMatArr_col2 = ANGLE_output_global._uoutMatArr[2];
  return ANGLE_output_annotated;
})";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, BVecsUniforms)
{
    const std::string &shaderString =
        R"(#version 300 es
precision mediump float;

uniform bvec2 uni_bvec;

uniform bvec2[2] uni_bvec_array;

out vec4 fragColor;

void main() {
  bvec2[2] local_bvec_array = uni_bvec_array;
  if (uni_bvec_array[0] == uni_bvec && uni_bvec == local_bvec_array[1]) {
    fragColor = vec4(1.0);
  } else {
    fragColor = vec4(0.0);
  }
}
)";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Output_Global {
  _ufragColor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _ufragColor : vec4<f32>,
};

@group(0) @binding(1) var<uniform> ANGLE_defaultUniformBlock : ANGLE_DefaultUniformBlock;
@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLE_wrapped_uvec2
{
  @align(16) elem : vec2<u32>
};
fn ANGLE_Convert_Array2_ANGLE_wrapped_uvec2_ElementsTo_bvec2_Elements(wrappedArr : array<ANGLE_wrapped_uvec2, 2>) -> array<vec2<bool>, 2>
{
  var retVal : array<vec2<bool>, 2>;
  for (var i : u32 = 0; i < 2; i++) {;
    retVal[i] = (vec2<u32>(0u) != wrappedArr[i].elem);
  }
  return retVal;
}
struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

struct ANGLE_DefaultUniformBlock
{
  @align(16) _uuni_bvec : vec2<u32>,
  @align(16) _uuni_bvec_array : array<ANGLE_wrapped_uvec2, 2>,
};

;

fn _umain()
{
  var _ulocal_bvec_array : array<vec2<bool>, 2> = (ANGLE_Convert_Array2_ANGLE_wrapped_uvec2_ElementsTo_bvec2_Elements((ANGLE_defaultUniformBlock)._uuni_bvec_array));
  if ((all(((vec2<u32>(0u) != (ANGLE_defaultUniformBlock)._uuni_bvec_array[0i].elem)) == ((vec2<u32>(0u) != (ANGLE_defaultUniformBlock)._uuni_bvec)))) && (all(((vec2<u32>(0u) != (ANGLE_defaultUniformBlock)._uuni_bvec)) == ((_ulocal_bvec_array)[1i]))))
  {
    (ANGLE_output_global._ufragColor) = (vec4<f32>(1.0f, 1.0f, 1.0f, 1.0f));
  }
  else
  {
    (ANGLE_output_global._ufragColor) = (vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f));
  }
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._ufragColor = ANGLE_output_global._ufragColor;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}

TEST_F(WGSLOutputTest, DefaultUniformsNoStructWrapper)
{
    const std::string &shaderString =
        R"(#version 300 es
precision mediump float;

uniform float x1;
uniform int y1;
uniform vec2 z1;
uniform mat3x2 a1;
uniform mat4x4 b1;

uniform float[2] x2;
uniform int[2] y2;
uniform vec2[2] z2;
uniform mat3x2[2] a2;
uniform mat4x4[2] b2;

out vec4 fragColor;
void main() {
  fragColor = vec4(x1 + float(y1), z1.x, a1[0][0], b1[0][0]);
  fragColor += vec4(x2[0] + float(y2[0]), z2[0].x, a2[0][0][0], b2[0][0][0]);
}
)";
    const std::string &outputString =
        R"(diagnostic(warning,derivative_uniformity);
struct ANGLE_Output_Global {
  _ufragColor : vec4<f32>,
};

var<private> ANGLE_output_global : ANGLE_Output_Global;

struct ANGLE_Output_Annotated {
  @location(@@@@@@) _ufragColor : vec4<f32>,
};

@group(0) @binding(1) var<uniform> ANGLE_defaultUniformBlock : ANGLE_DefaultUniformBlock;
@group(2) @binding(0) var<uniform> ANGLEUniforms : ANGLEUniformBlock;

struct ANGLE_wrapped_float
{
  @align(16) elem : f32
};
struct ANGLE_wrapped_vec2
{
  @align(16) elem : vec2<f32>
};
struct ANGLE_wrapped_int
{
  @align(16) elem : i32
};
fn ANGLE_Convert_Mat3x2(mangledMatrix : array<ANGLE_wrapped_vec2, 3>) -> mat3x2<f32>
{
  var retVal : mat3x2<f32>;
  retVal = mat3x2<f32>(mangledMatrix[0].elem, mangledMatrix[1].elem, mangledMatrix[2].elem);
  return retVal;
}
struct ANGLEDepthRangeParams
{
  near : f32,
  far : f32,
  diff : f32,
};

;

struct ANGLEUniformBlock
{
  @align(16) acbBufferOffsets : vec2<u32>,
  depthRange : vec2<f32>,
  renderArea : u32,
  flipXY : u32,
  dither : u32,
  misc : u32,
};

;

struct ANGLE_DefaultUniformBlock
{
  @align(16) _ux1 : f32,
  _uy1 : i32,
  _uz1 : vec2<f32>,
  @align(16) _ua1 : array<ANGLE_wrapped_vec2, 3>,
  _ub1 : mat4x4<f32>,
  @align(16) _ux2 : array<ANGLE_wrapped_float, 2>,
  @align(16) _uy2 : array<ANGLE_wrapped_int, 2>,
  @align(16) _uz2 : array<ANGLE_wrapped_vec2, 2>,
  @align(16) _ua2 : array<array<ANGLE_wrapped_vec2, 3>, 2>,
  @align(16) _ub2 : array<mat4x4<f32>, 2>,
};

;

fn _umain()
{
  (ANGLE_output_global._ufragColor) = (vec4<f32>(((ANGLE_defaultUniformBlock)._ux1) + (f32((ANGLE_defaultUniformBlock)._uy1)), ((ANGLE_defaultUniformBlock)._uz1).x, (ANGLE_Convert_Mat3x2((ANGLE_defaultUniformBlock)._ua1)[0i])[0i], (((ANGLE_defaultUniformBlock)._ub1)[0i])[0i]));
  (ANGLE_output_global._ufragColor) += (vec4<f32>(((ANGLE_defaultUniformBlock)._ux2[0i].elem) + (f32((ANGLE_defaultUniformBlock)._uy2[0i].elem)), ((ANGLE_defaultUniformBlock)._uz2[0i].elem).x, ((ANGLE_Convert_Mat3x2((ANGLE_defaultUniformBlock)._ua2[0i]))[0i])[0i], ((((ANGLE_defaultUniformBlock)._ub2)[0i])[0i])[0i]));
}
@fragment
fn wgslMain() -> ANGLE_Output_Annotated
{
  _umain();
  var ANGLE_output_annotated : ANGLE_Output_Annotated;
  ANGLE_output_annotated._ufragColor = ANGLE_output_global._ufragColor;
  return ANGLE_output_annotated;
}
)";
    compile(shaderString);
    EXPECT_TRUE(foundInCode(outputString.c_str()));
}
