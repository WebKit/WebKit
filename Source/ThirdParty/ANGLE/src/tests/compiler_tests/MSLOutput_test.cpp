//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MSLOutput_test.cpp:
//   Tests for MSL output.
//

#include <regex>
#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "gtest/gtest.h"
#include "tests/test_utils/compiler_test.h"

using namespace sh;

class MSLVertexOutputTest : public MatchOutputCodeTest
{
  public:
    MSLVertexOutputTest() : MatchOutputCodeTest(GL_VERTEX_SHADER, SH_MSL_METAL_OUTPUT)
    {
        ShCompileOptions defaultCompileOptions = {};
        setDefaultCompileOptions(defaultCompileOptions);
    }
};

class MSLOutputTest : public MatchOutputCodeTest
{
  public:
    MSLOutputTest() : MatchOutputCodeTest(GL_FRAGMENT_SHADER, SH_MSL_METAL_OUTPUT)
    {
        ShCompileOptions defaultCompileOptions = {};
        setDefaultCompileOptions(defaultCompileOptions);
    }
};

// Test that having dynamic indexing of a vector inside the right hand side of logical or doesn't
// trigger asserts in MSL output.
TEST_F(MSLOutputTest, DynamicIndexingOfVectorOnRightSideOfLogicalOr)
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

// Test that having an array constructor as a statement doesn't trigger an assert in MSL output.
TEST_F(MSLOutputTest, ArrayConstructorStatement)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;
        out vec4 outColor;
        void main()
        {
            outColor = vec4(0.0, 0.0, 0.0, 1.0);
            float[1](outColor[1]++);
        })";
    compile(shaderString);
}

// Test an array of arrays constructor as a statement.
TEST_F(MSLOutputTest, ArrayOfArraysStatement)
{
    const std::string &shaderString =
        R"(#version 310 es
        precision mediump float;
        out vec4 outColor;
        void main()
        {
            outColor = vec4(0.0, 0.0, 0.0, 1.0);
            float[2][2](float[2](outColor[1]++, 0.0), float[2](1.0, 2.0));
        })";
    compile(shaderString);
}

// Test dynamic indexing of a vector. This makes sure that helper functions added for dynamic
// indexing have correct data that subsequent traversal steps rely on.
TEST_F(MSLOutputTest, VectorDynamicIndexing)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;
        out vec4 outColor;
        uniform int i;
        void main()
        {
            vec4 foo = vec4(0.0, 0.0, 0.0, 1.0);
            foo[i] = foo[i + 1];
            outColor = foo;
        })";
    compile(shaderString);
}

// Test returning an array from a user-defined function. This makes sure that function symbols are
// changed consistently when the user-defined function is changed to have an array out parameter.
TEST_F(MSLOutputTest, ArrayReturnValue)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;
        uniform float u;
        out vec4 outColor;

        float[2] getArray(float f)
        {
            return float[2](f, f + 1.0);
        }

        void main()
        {
            float[2] arr = getArray(u);
            outColor = vec4(arr[0], arr[1], 0.0, 1.0);
        })";
    compile(shaderString);
}

// Test that writing parameters without a name doesn't assert.
TEST_F(MSLOutputTest, ParameterWithNoName)
{
    const std::string &shaderString =
        R"(precision mediump float;

        uniform vec4 v;

        vec4 s(vec4)
        {
            return v;
        }
        void main()
        {
            gl_FragColor = s(v);
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, Macro)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        #define FOO vec4

        out vec4 outColor;

        void main()
        {
            outColor = FOO(1.0, 2.0, 3.0, 4.0);
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, UniformSimple)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        out vec4 outColor;
        uniform float x;

        void main()
        {
            outColor = vec4(x, x, x, x);
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, FragmentOutSimple)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        out vec4 outColor;

        void main()
        {
            outColor = vec4(1.0, 2.0, 3.0, 4.0);
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, FragmentOutIndirect1)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        out vec4 outColor;

        void foo()
        {
            outColor = vec4(1.0, 2.0, 3.0, 4.0);
        }

        void bar()
        {
            foo();
        }

        void main()
        {
            bar();
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, FragmentOutIndirect2)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        out vec4 outColor;

        void foo();

        void bar()
        {
            foo();
        }

        void foo()
        {
            outColor = vec4(1.0, 2.0, 3.0, 4.0);
        }

        void main()
        {
            bar();
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, FragmentOutIndirect3)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        out vec4 outColor;

        float foo(float x, float y)
        {
            outColor = vec4(x, y, 3.0, 4.0);
            return 7.0;
        }

        float bar(float x)
        {
            return foo(x, 2.0);
        }

        float baz()
        {
            return 13.0;
        }

        float identity(float x)
        {
            return x;
        }

        void main()
        {
            identity(bar(baz()));
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, VertexInOut)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;
        in float in0;
        out float out0;
        void main()
        {
            out0 = in0;
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, SymbolSharing)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        out vec4 outColor;

        struct Foo {
            float x;
            float y;
        };

        void doFoo(Foo foo, float zw);

        void doFoo(Foo foo, float zw)
        {
            foo.x = foo.y;
            outColor = vec4(foo.x, foo.y, zw, zw);
        }

        void main()
        {
            Foo foo;
            foo.x = 2.0;
            foo.y = 2.0;
            doFoo(foo, 3.0);
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, StructDecl)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        out float out0;

        struct Foo {
            float value;
        };

        void main()
        {
            Foo foo;
            out0 = foo.value;
        }
        )";
    compile(shaderString);
}

TEST_F(MSLOutputTest, Structs)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        struct Foo {
            float value;
        };

        out vec4 out0;

        struct Bar {
            Foo foo;
        };

        void go();

        uniform UniInstance {
            Bar bar;
            float instance;
        } uniInstance;

        uniform UniGlobal {
            Foo foo;
            float global;
        };

        void main()
        {
            go();
        }

        struct Baz {
            Bar bar;
        } baz;

        void go()
        {
            out0.x = baz.bar.foo.value;
            out0.y = global;
            out0.z = uniInstance.instance;
            out0.w = 0.0;
        }

        )";
    compile(shaderString);
}

TEST_F(MSLOutputTest, KeywordConflict)
{
    const std::string &shaderString =
        R"(#version 300 es
            precision highp float;

        struct fragment {
            float kernel;
        } device;

        struct Foo {
            fragment frag;
        } foo;

        out float vertex;
        float kernel;

        float stage_in(float x)
        {
            return x;
        }

        void metal(float metal, float fragment);
        void metal(float metal, float fragment)
        {
            vertex = metal * fragment * foo.frag.kernel;
        }

        void main()
        {
            metal(stage_in(stage_in(kernel * device.kernel)), foo.frag.kernel);
        })";
    compile(shaderString);
}

TEST_F(MSLVertexOutputTest, Vertex)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;
        void main()
        {
            gl_Position = vec4(1.0,1.0,1.0,1.0);
        })";
    compile(shaderString);
}

TEST_F(MSLVertexOutputTest, LastReturn)
{
    const std::string &shaderString =
        R"(#version 300 es
        in highp vec4 a_position;
        in highp vec4 a_coords;
        out highp vec4 v_color;

        void main (void)
        {
            gl_Position = a_position;
            v_color = vec4(a_coords.xyz, 1.0);
            return;
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, LastReturn)
{
    const std::string &shaderString =
        R"(#version 300 es
        in mediump vec4 v_coords;
        layout(location = 0) out mediump vec4 o_color;

        void main (void)
        {
            o_color = vec4(v_coords.xyz, 1.0);
            return;
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, FragColor)
{
    const std::string &shaderString = R"(
        void main ()
        {
            gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, MatrixIn)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;

        in mat4 mat;
        out float out0;

        void main()
        {
            out0 = mat[0][0];
        }
        )";
    compile(shaderString);
}

TEST_F(MSLOutputTest, WhileTrue)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;

        uniform float uf;
        out vec4 my_FragColor;

        void main()
        {
            my_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            while (true)
            {
                break;
            }
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, ForTrue)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;

        uniform float uf;
        out vec4 my_FragColor;

        void main()
        {
            my_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            for (;true;)
            {
                break;
            }
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, ForEmpty)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;

        uniform float uf;
        out vec4 my_FragColor;

        void main()
        {
            my_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            for (;;)
            {
                break;
            }
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, ForComplex)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;

        uniform float uf;
        out vec4 my_FragColor;

        void main()
        {
            my_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            for (int i = 0, j = 2; i < j; ++i) {
                if (i == 0) continue;
                if (i == 42) break;
                my_FragColor.x += float(i);
            }
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, ForSymbol)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;

        uniform float uf;
        out vec4 my_FragColor;

        void main()
        {
            bool cond = true;
            for (;cond;)
            {
                my_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
                cond = false;
            }
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, DoWhileSymbol)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;

        uniform float uf;
        out vec4 my_FragColor;

        void main()
        {
            bool cond = false;
            do
            {
                my_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            } while (cond);
        })";
    compile(shaderString);
}

TEST_F(MSLOutputTest, AnonymousStruct)
{
    const std::string &shaderString =
        R"(
        precision mediump float;
        struct { vec4 v; } anonStruct;
        void main() {
            anonStruct.v = vec4(0.0,1.0,0.0,1.0);
            gl_FragColor = anonStruct.v;
        })";
    compile(shaderString);
    // TODO(anglebug.com/6395): This success condition is expected to fail now.
    // When WebKit build is able to run the tests, this should be changed to something else.
    //    ASSERT_TRUE(foundInCode(SH_MSL_METAL_OUTPUT, "__unnamed"));
}
