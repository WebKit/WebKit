//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "test_utils/CompilerTest.h"
#include "test_utils/angle_test_configs.h"

using namespace angle;

namespace
{
class GLSLValidationTest : public CompilerTest
{
  protected:
    GLSLValidationTest() {}

    // Helper to create a shader, then verify that it fails to compile with the given reason.  It's
    // given:
    //
    // * The type of shader.
    // * The shader source itself.
    // * An error string to look for in the compile logs.
    void validateError(GLenum shaderType, const char *shaderSource, const char *expectedError)
    {
        const CompiledShader &shader = compile(shaderType, shaderSource);
        EXPECT_FALSE(shader.success());

        EXPECT_TRUE(shader.hasInfoLog(expectedError)) << expectedError;
        reset();
    }

    // Helper to create a shader, then verify that compilation succeeded.
    void validateSuccess(GLenum shaderType, const char *shaderSource)
    {
        const CompiledShader &shader = compile(shaderType, shaderSource);
        EXPECT_TRUE(shader.success());
        reset();
    }

    void validateWarning(GLenum shaderType, const char *shaderSource, const char *expectedWarning)
    {
        const CompiledShader &shader = compile(shaderType, shaderSource);
        EXPECT_TRUE(shader.success());

        EXPECT_TRUE(shader.hasInfoLog(expectedWarning)) << expectedWarning;
        reset();
    }
};

class GLSLValidationTest_ES3 : public GLSLValidationTest
{};

class GLSLValidationTest_ES31 : public GLSLValidationTest
{};

class GLSLValidationTestNoValidation : public GLSLValidationTest
{
  public:
    GLSLValidationTestNoValidation() { setNoErrorEnabled(true); }
};

class WebGLGLSLValidationTest : public GLSLValidationTest
{
  protected:
    WebGLGLSLValidationTest() { setWebGLCompatibilityEnabled(true); }
};

class WebGL2GLSLValidationTest : public GLSLValidationTest_ES3
{
  protected:
    WebGL2GLSLValidationTest() { setWebGLCompatibilityEnabled(true); }

    void testInfiniteLoop(const char *fs)
    {
        const CompiledShader &shader = compile(GL_FRAGMENT_SHADER, fs);
        if (getEGLWindow()->isFeatureEnabled(Feature::RejectWebglShadersWithUndefinedBehavior))
        {
            EXPECT_FALSE(shader.success());
        }
        else
        {
            EXPECT_TRUE(shader.success());
        }
        reset();
    }
};

// Test that an empty shader fails to compile
TEST_P(GLSLValidationTest, EmptyShader)
{
    constexpr char kFS[] = "";
    validateError(GL_FRAGMENT_SHADER, kFS, "syntax error");
}

// Test that a shader with no main in it fails to compile
TEST_P(GLSLValidationTest, MissingMain)
{
    constexpr char kFS[] = R"(precision mediump float;)";
    validateError(GL_FRAGMENT_SHADER, kFS, "Missing main()");
}

// Test that a shader with only a main prototype in it fails to compile
TEST_P(GLSLValidationTest, MainPrototypeOnly)
{
    constexpr char kFS[] = R"(precision mediump float;
void main();
)";
    validateError(GL_FRAGMENT_SHADER, kFS, "Missing main()");
}

// Test relational operations between bools is rejected.
TEST_P(GLSLValidationTest, BoolLessThan)
{
    constexpr char kFS[] = R"(uniform mediump vec4 u;
void main() {
  bool a = bool(u.x);
  bool b = bool(u.y);
  bool c = a < b;
  gl_FragColor = vec4(c, !c, c, !c);
}
)";
    validateError(GL_FRAGMENT_SHADER, kFS, "'<' : comparison operator not defined for booleans");
}

// This is a test for a bug that used to exist in ANGLE:
// Calling a function with all parameters missing should not succeed.
TEST_P(GLSLValidationTest, FunctionParameterMismatch)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        float fun(float a) {
            return a * 2.0;
        }
        void main() {
            float ff = fun();
            gl_FragColor = vec4(ff);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS, "'fun' : no matching overloaded function found");
}

// Functions can't be redeclared as variables in the same scope (ESSL 1.00 section 4.2.7)
TEST_P(GLSLValidationTest, RedeclaringFunctionAsVariable)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        float fun(float a) {
            return a * 2.0;
        }

        float fun;
        void main() {
             gl_FragColor = vec4(0.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS, "'fun' : redefinition");
}

// Functions can't be redeclared as structs in the same scope (ESSL 1.00 section 4.2.7)
TEST_P(GLSLValidationTest, RedeclaringFunctionAsStruct)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        float fun(float a) {
           return a * 2.0;
        }
        struct fun { float a; };
        void main() {
           gl_FragColor = vec4(0.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS, "'fun' : redefinition of a struct");
}

// Functions can't be redeclared with different qualifiers (ESSL 1.00 section 6.1.0)
TEST_P(GLSLValidationTest, RedeclaringFunctionWithDifferentQualifiers)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        float fun(out float a);
        float fun(float a) {
           return a * 2.0;
        }
        void main() {
           gl_FragColor = vec4(0.0);
        }
    )";

    validateError(
        GL_FRAGMENT_SHADER, kFS,
        "'in' : function must have the same parameter qualifiers in all of its declarations");
}

// Assignment and equality are undefined for structures containing arrays (ESSL 1.00 section 5.7)
TEST_P(GLSLValidationTest, CompareStructsContainingArrays)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        struct s { float a[3]; };
        void main() {
           s a;
           s b;
           bool c = (a == b);
           gl_FragColor = vec4(c ? 1.0 : 0.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'==' : undefined operation for structs containing arrays");
}

// Assignment and equality are undefined for structures containing arrays (ESSL 1.00 section 5.7)
TEST_P(GLSLValidationTest, AssignStructsContainingArrays)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        struct s { float a[3]; };
        void main() {
           s a;
           s b;
           b.a[0] = 0.0;
           a = b;
           gl_FragColor = vec4(a.a[0]);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'=' : undefined operation for structs containing arrays");
}

// Assignment and equality are undefined for structures containing samplers (ESSL 1.00 sections 5.7
// and 5.9)
TEST_P(GLSLValidationTest, CompareStructsContainingSamplers)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        struct s { sampler2D foo; };
        uniform s a;
        uniform s b;
        void main() {
           bool c = (a == b);
           gl_FragColor = vec4(c ? 1.0 : 0.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'==' : undefined operation for structs containing samplers");
}

// Samplers are not allowed as l-values (ESSL 3.00 section 4.1.7), our interpretation is that this
// extends to structs containing samplers. ESSL 1.00 spec is clearer about this.
TEST_P(GLSLValidationTest_ES3, AssignStructsContainingSamplers)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        struct s { sampler2D foo; };
        uniform s a;
        out vec4 my_FragColor;
        void main() {
           s b;
           b = a;
           my_FragColor = vec4(1.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'structure' : structures must be uniform (structure contains a sampler)");
}

// This is a regression test for a particular bug that was in ANGLE.
// It also verifies that ESSL3 functionality doesn't leak to ESSL1.
TEST_P(GLSLValidationTest, ArrayWithNoSizeInInitializerList)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        void main() {
           float a[2], b[];
           gl_FragColor = vec4(1.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  " '[]' : implicitly sized array supported in GLSL ES 3.00 and above only");
}

// Const variables need an initializer.
TEST_P(GLSLValidationTest_ES3, ConstVarNotInitialized)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        out vec4 my_FragColor;
        void main() {
           const float a;
           my_FragColor = vec4(1.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'a' : variables with qualifier 'const' must be initialized");
}

// Const variables need an initializer. In ESSL1 const structs containing
// arrays are not allowed at all since it's impossible to initialize them.
// Even though this test is for ESSL3 the only thing that's critical for
// ESSL1 is the non-initialization check that's used for both language versions.
// Whether ESSL1 compilation generates the most helpful error messages is a
// secondary concern.
TEST_P(GLSLValidationTest_ES3, ConstStructNotInitialized)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        struct S { float a[3]; };
        out vec4 my_FragColor;
        void main() {
           const S b;
           my_FragColor = vec4(1.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'b' : variables with qualifier 'const' must be initialized");
}

// Const variables need an initializer. In ESSL1 const arrays are not allowed
// at all since it's impossible to initialize them.
// Even though this test is for ESSL3 the only thing that's critical for
// ESSL1 is the non-initialization check that's used for both language versions.
// Whether ESSL1 compilation generates the most helpful error messages is a
// secondary concern.
TEST_P(GLSLValidationTest_ES3, ConstArrayNotInitialized)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        out vec4 my_FragColor;
        void main() {
           const float a[3];
           my_FragColor = vec4(1.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'a' : variables with qualifier 'const' must be initialized");
}

// Block layout qualifiers can't be used on non-block uniforms (ESSL 3.00 section 4.3.8.3)
TEST_P(GLSLValidationTest_ES3, BlockLayoutQualifierOnRegularUniform)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        layout(packed) uniform mat2 x;
        out vec4 my_FragColor;
        void main() {
           my_FragColor = vec4(1.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'packed' : layout qualifier only valid for interface blocks");
}

// Block layout qualifiers can't be used on non-block uniforms (ESSL 3.00 section 4.3.8.3)
TEST_P(GLSLValidationTest_ES3, BlockLayoutQualifierOnUniformWithEmptyDecl)
{
    // Yes, the comma in the declaration below is not a typo.
    // Empty declarations are allowed in GLSL.
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        layout(packed) uniform mat2, x;
        out vec4 my_FragColor;
        void main() {
           my_FragColor = vec4(1.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'packed' : layout qualifier only valid for interface blocks");
}

// Arrays of arrays are not allowed (ESSL 3.00 section 4.1.9)
TEST_P(GLSLValidationTest_ES3, ArraysOfArrays1)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        out vec4 my_FragColor;
        void main() {
           float[5] a[3];
           my_FragColor = vec4(1.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'mediump array[5] of float' : cannot declare arrays of arrays");
}

// Arrays of arrays are not allowed (ESSL 3.00 section 4.1.9)
TEST_P(GLSLValidationTest_ES3, ArraysOfArrays2)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        out vec4 my_FragColor;
        void main() {
           float[2] a, b[3];
           my_FragColor = vec4(1.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "mediump array[2] of float' : cannot declare arrays of arrays");
}

// Arrays of arrays are not allowed (ESSL 3.00 section 4.1.9). Test this in a struct.
TEST_P(GLSLValidationTest_ES3, ArraysOfArraysInStruct)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        out vec4 my_FragColor;
        struct S { float[2] foo[3]; };
        void main() { my_FragColor = vec4(1.0); }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'mediump array[2] of float' : cannot declare arrays of arrays");
}

// Test invalid dimensionality of implicitly sized array constructor arguments.
TEST_P(GLSLValidationTest_ES31,
       TooHighDimensionalityOfImplicitlySizedArrayOfArraysConstructorArguments)
{
    constexpr char kFS[] = R"(#version 310 es
        precision mediump float;
        out vec4 my_FragColor;
        void main() {
            float[][] a = float[][](float[1][1](float[1](1.0)), float[1][1](float[1](2.0)));
            my_FragColor = vec4(a[0][0]);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'constructor' : constructing from a non-dereferenced array");
}

// Test invalid dimensionality of implicitly sized array constructor arguments.
TEST_P(GLSLValidationTest_ES31,
       TooLowDimensionalityOfImplicitlySizedArrayOfArraysConstructorArguments)
{
    constexpr char kFS[] = R"(#version 310 es
        precision mediump float;
        out vec4 my_FragColor;
        void main() {
            float[][][] a = float[][][](float[2](1.0, 2.0), float[2](3.0, 4.0));
            my_FragColor = vec4(a[0][0][0]);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'constructor' : implicitly sized array of arrays constructor argument "
                  "dimensionality is too low");
}

// Implicitly sized arrays need to be initialized (ESSL 3.00 section 4.1.9)
TEST_P(GLSLValidationTest_ES3, UninitializedImplicitArraySize)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        out vec4 my_FragColor;
        void main() {
           float[] a;
           my_FragColor = vec4(1.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'a' : implicitly sized arrays only allowed for tessellation shaders or geometry "
                  "shader inputs");
}

// An operator can only form a constant expression if all the operands are constant expressions
// - even operands of ternary operator that are never evaluated. (ESSL 3.00 section 4.3.3)
TEST_P(GLSLValidationTest_ES3, TernaryOperatorNotConstantExpression)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        out vec4 my_FragColor;
        uniform bool u;
        void main() {
           const bool a = true ? true : u;
           my_FragColor = vec4(1.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS, "'=' : assigning non-constant to 'const bool'");
}

// Ternary operator can operate on arrays (ESSL 3.00 section 5.7)
TEST_P(GLSLValidationTest_ES3, TernaryOperatorOnArrays)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        out vec4 my_FragColor;
        void main() {
           float[1] a = float[1](0.0);
           float[1] b = float[1](1.0);
           float[1] c = true ? a : b;
           my_FragColor = vec4(1.0);
        }
    )";

    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Ternary operator can operate on structs (ESSL 3.00 section 5.7)
TEST_P(GLSLValidationTest_ES3, TernaryOperatorOnStructs)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        out vec4 my_FragColor;
        struct S { float foo; };
        void main() {
           S a = S(0.0);
           S b = S(1.0);
           S c = true ? a : b;
           my_FragColor = vec4(1.0);
        }
    )";

    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Array length() returns a constant signed integral expression (ESSL 3.00 section 4.1.9)
// Assigning it to unsigned should result in an error.
TEST_P(GLSLValidationTest_ES3, AssignArrayLengthToUnsigned)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        out vec4 my_FragColor;
        void main() {
           int[1] arr;
           uint l = arr.length();
           my_FragColor = vec4(float(l));
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'=' : cannot convert from 'const highp int' to 'mediump uint'");
}

// Global variable initializers need to be constant expressions (ESSL 1.00 section 4.3)
// Initializing with a varying should be an error.
TEST_P(GLSLValidationTest, AssignVaryingToGlobal)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        varying float a;
        float b = a * 2.0;
        void main() {
           gl_FragColor = vec4(b);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'=' : global variable initializers must be constant expressions");
}

// Global variable initializers need to be constant expressions (ESSL 3.00 section 4.3)
// Initializing with an uniform should be an error.
TEST_P(GLSLValidationTest_ES3, AssignUniformToGlobalESSL3)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        uniform float a;
        float b = a * 2.0;
        out vec4 my_FragColor;
        void main() {
           my_FragColor = vec4(b);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'=' : global variable initializers must be constant expressions");
}

// Global variable initializers need to be constant expressions (ESSL 1.00 section 4.3)
// Initializing with an uniform used to generate a warning on ESSL 1.00 because of legacy
// compatibility, but that causes dEQP to fail (which expects an error)
TEST_P(GLSLValidationTest, AssignUniformToGlobalESSL1)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        uniform float a;
        float b = a * 2.0;
        void main() {
           gl_FragColor = vec4(b);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'=' : global variable initializers must be constant expressions");
}

// Global variable initializers need to be constant expressions (ESSL 1.00 section 4.3)
// Initializing with an user-defined function call should be an error.
TEST_P(GLSLValidationTest, AssignFunctionCallToGlobal)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        float foo() { return 1.0; }
        float b = foo();
        void main() {
           gl_FragColor = vec4(b);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'=' : global variable initializers must be constant expressions");
}

// Global variable initializers need to be constant expressions (ESSL 1.00 section 4.3)
// Initializing with an assignment to another global should be an error.
TEST_P(GLSLValidationTest, AssignAssignmentToGlobal)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        float c = 1.0;
        float b = (c = 0.0);
        void main() {
           gl_FragColor = vec4(b);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  " '=' : global variable initializers must be constant expressions");
}

// Global variable initializers need to be constant expressions (ESSL 1.00 section 4.3)
// Initializing with incrementing another global should be an error.
TEST_P(GLSLValidationTest, AssignIncrementToGlobal)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        float c = 1.0;
        float b = (c++);
        void main() {
           gl_FragColor = vec4(b);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  " '=' : global variable initializers must be constant expressions");
}

// Global variable initializers need to be constant expressions (ESSL 1.00 section 4.3)
// Initializing with an assignment to another global should be an error.
TEST_P(GLSLValidationTest, AssignTexture2DToGlobal)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        uniform mediump sampler2D s;
        float b = texture2D(s, vec2(0.5, 0.5)).x;
        void main() {
           gl_FragColor = vec4(b);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'=' : global variable initializers must be constant expressions");
}

// Global variable initializers need to be constant expressions (ESSL 3.00 section 4.3)
// Initializing with a non-constant global should be an error.
TEST_P(GLSLValidationTest_ES3, AssignNonConstGlobalToGlobal)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        float a = 1.0;
        float b = a * 2.0;
        out vec4 my_FragColor;
        void main() {
           my_FragColor = vec4(b);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'=' : global variable initializers must be constant expressions");
}

// Global variable initializers need to be constant expressions (ESSL 3.00 section 4.3)
// Initializing with a constant global should be fine.
TEST_P(GLSLValidationTest_ES3, AssignConstGlobalToGlobal)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        const float a = 1.0;
        float b = a * 2.0;
        out vec4 my_FragColor;
        void main() {
           my_FragColor = vec4(b);
        }
    )";

    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Statically assigning to both gl_FragData and gl_FragColor is forbidden (ESSL 1.00 section 7.2)
TEST_P(GLSLValidationTest, WriteBothFragDataAndFragColor)
{
    constexpr char kFS[] = R"(
        precision mediump float;
        void foo() {
           gl_FragData[0].a++;
        }
        void main() {
           gl_FragColor.x += 0.0;
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS, "cannot use both gl_FragData and gl_FragColor");
}

// Version directive must be on the first line (ESSL 3.00 section 3.3)
TEST_P(GLSLValidationTest_ES3, VersionOnSecondLine)
{
    constexpr char kFS[] = R"(
        #version 300 es
        precision mediump float;
        out vec4 my_FragColor;
        void main() {
           my_FragColor = vec4(0.0);
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "#version directive must occur on the first line of the shader");
}

// Layout qualifier can only appear in global scope (ESSL 3.00 section 4.3.8)
TEST_P(GLSLValidationTest_ES3, LayoutQualifierInCondition)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        uniform vec4 u;
        out vec4 my_FragColor;
        void main() {
            int i = 0;
            for (int j = 0; layout(location = 0) bool b = false; ++j) {
                ++i;
            }
            my_FragColor = u;
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS, "'layout' : only allowed at global scope");
}

// Layout qualifier can only appear where specified (ESSL 3.00 section 4.3.8)
TEST_P(GLSLValidationTest_ES3, LayoutQualifierInFunctionReturnType)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        uniform vec4 u;
        out vec4 my_FragColor;
        layout(location = 0) vec4 foo() {
            return u;
        }
        void main() {
            my_FragColor = foo();
        }
    )";

    validateError(GL_FRAGMENT_SHADER, kFS, "'layout' : no qualifiers allowed for function return");
}

// If there is more than one output, the location must be specified for all outputs.
// (ESSL 3.00.04 section 4.3.8.2)
TEST_P(GLSLValidationTest_ES3, TwoOutputsNoLayoutQualifiers)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        uniform vec4 u;
        out vec4 my_FragColor;
        out vec4 my_SecondaryFragColor;
        void main() {
            my_FragColor = vec4(1.0);
            my_SecondaryFragColor = vec4(0.5);
        }
      )";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'my_FragColor' : must explicitly specify all locations when using multiple "
                  "fragment outputs");
}

// (ESSL 3.00.04 section 4.3.8.2)
TEST_P(GLSLValidationTest_ES3, TwoOutputsFirstLayoutQualifier)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        uniform vec4 u;
        layout(location = 0) out vec4 my_FragColor;
        out vec4 my_SecondaryFragColor;
        void main() {
            my_FragColor = vec4(1.0);
            my_SecondaryFragColor = vec4(0.5);
        })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'my_SecondaryFragColor' : must explicitly specify all locations when using "
                  "multiple fragment outputs");
}

// (ESSL 3.00.04 section 4.3.8.2)
TEST_P(GLSLValidationTest_ES3, TwoOutputsSecondLayoutQualifier)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        uniform vec4 u;
        out vec4 my_FragColor;
        layout(location = 0) out vec4 my_SecondaryFragColor;
        void main() {
            my_FragColor = vec4(1.0);
            my_SecondaryFragColor = vec4(0.5);
    })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'my_FragColor' : must explicitly specify all locations when using multiple "
                  "fragment outputs");
}

// Uniforms can be arrays (ESSL 3.00 section 4.3.5)
TEST_P(GLSLValidationTest_ES3, UniformArray)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        uniform vec4[2] u;
        out vec4 my_FragColor;
        void main() {
            my_FragColor = u[0];
      })";

    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Fragment shader input variables cannot be arrays of structs (ESSL 3.00 section 4.3.4)
TEST_P(GLSLValidationTest_ES3, FragmentInputArrayOfStructs)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        struct S {
            vec4 foo;
        };
        in S i[2];
        out vec4 my_FragColor;
        void main() {
            my_FragColor = i[0].foo;
      })";

    validateError(GL_FRAGMENT_SHADER, kFS, "cannot declare arrays of structs of this qualifier");
}

// Vertex shader inputs can't be arrays (ESSL 3.00 section 4.3.4)
// This test is testing the case where the array brackets are after the variable name, so
// the arrayness isn't known when the type and qualifiers are initially parsed.
TEST_P(GLSLValidationTest_ES3, VertexShaderInputArray)
{
    constexpr char kVS[] = R"(#version 300 es
        precision mediump float;
        in vec4 i[2];
        void main() {
            gl_Position = i[0];
        })";

    validateError(GL_VERTEX_SHADER, kVS, "'in' : cannot declare arrays of this qualifier");
}

// Vertex shader inputs can't be arrays (ESSL 3.00 section 4.3.4)
// This test is testing the case where the array brackets are after the type.
TEST_P(GLSLValidationTest_ES3, VertexShaderInputArrayType)
{
    constexpr char kVS[] = R"(#version 300 es
        precision mediump float;
        in vec4[2] i;
        void main() {
            gl_Position = i[0];
        })";

    validateError(GL_VERTEX_SHADER, kVS, "'in' : cannot be array");
}

// Fragment shader inputs can't contain booleans (ESSL 3.00 section 4.3.4)
TEST_P(GLSLValidationTest_ES3, FragmentShaderInputStructWithBool)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        struct S { bool foo; };
        in S s;
        out vec4 my_FragColor;
        void main() {
            my_FragColor = vec4(0.0);
        })";

    validateError(GL_FRAGMENT_SHADER, kFS, " 'in' : cannot be a structure containing a bool");
}

// Fragment shader inputs without a flat qualifier can't contain integers (ESSL 3.00 section 4.3.4)
TEST_P(GLSLValidationTest_ES3, FragmentShaderInputStructWithInt)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        struct S { int foo; };
        in S s;
        out vec4 my_FragColor;
        void main() {
            my_FragColor = vec4(0.0);
        })";

    validateError(GL_FRAGMENT_SHADER, kFS, "'in' : must use 'flat' interpolation here");
}

// Test that out-of-range integer literal generates an error in ESSL 3.00.
TEST_P(GLSLValidationTest_ES3, OutOfRangeIntegerLiteral)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        precision highp int;
        out vec4 my_FragColor;
        void main() {
            my_FragColor = vec4(0x100000000);
        })";

    validateError(GL_FRAGMENT_SHADER, kFS, "'0x100000000' : Integer overflow");
}

// Test that a ternary operator with one unevaluated non-constant operand is not a constant
// expression.
TEST_P(GLSLValidationTest, TernaryOperatorNonConstantOperand)
{
    constexpr char kFS[] = R"(precision mediump float;
        uniform float u;
        void main() {
            const float f = true ? 1.0 : u;
            gl_FragColor = vec4(f);
        })";

    validateError(GL_FRAGMENT_SHADER, kFS, "'=' : assigning non-constant to 'const mediump float'");
}

// Test that a sampler can't be used in constructor argument list
TEST_P(GLSLValidationTest, SamplerInConstructorArguments)
{
    constexpr char kFS[] = R"(precision mediump float;
        uniform sampler2D s;
        void main()
        {
            vec2 v = vec2(0.0, s);
            gl_FragColor = vec4(v, 0.0, 0.0);
        })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'constructor' : cannot convert a variable with type sampler2D");
}

// Test that void can't be used in constructor argument list
TEST_P(GLSLValidationTest, VoidInConstructorArguments)
{
    constexpr char kFS[] = R"(precision mediump float;
        void foo() {}
        void main()
        {
            vec2 v = vec2(0.0, foo());
            gl_FragColor = vec4(v, 0.0, 0.0);
        })";

    validateError(GL_FRAGMENT_SHADER, kFS, "'constructor' : cannot convert a void");
}

// Test that a shader with empty constructor parameter list is not accepted.
TEST_P(GLSLValidationTest_ES3, EmptyArrayConstructor)
{
    constexpr char kFS[] = R"(#version 300 es
         precision mediump float;
         out vec4 my_FragColor;
         uniform float u;
         const float[] f = float[]();
         void main() {
             my_FragColor = vec4(0.0);
      })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'[]' : implicitly sized array constructor must have at least one argument");
}

// Test that indexing fragment outputs with a non-constant expression is forbidden, even if ANGLE
// is able to constant fold the index expression. ESSL 3.00 section 4.3.6.
TEST_P(GLSLValidationTest_ES3, DynamicallyIndexedFragmentOutput)
{
    constexpr char kFS[] = R"(#version 300 es
         precision mediump float;
         uniform int a;
         out vec4[2] my_FragData;
         void main()
         {
             my_FragData[true ? 0 : a] = vec4(0.0);
         }
    )";

    validateError(
        GL_FRAGMENT_SHADER, kFS,
        " '[' : array indexes for fragment outputs must be constant integral expressions");
}

// Test that indexing a uniform buffer array with a non-constant expression is forbidden, even if
// ANGLE is able to constant fold the index expression. ESSL 3.00 section 4.3.7.
TEST_P(GLSLValidationTest_ES3, DynamicallyIndexedUniformBuffer)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        uniform int a;
        uniform B
        {
            vec4 f;
        }
        blocks[2];
        out vec4 my_FragColor;
        void main()
        {
            my_FragColor = blocks[true ? 0 : a].f;
        })";

    validateError(
        GL_FRAGMENT_SHADER, kFS,
        "'[' : array indexes for uniform block arrays must be constant integral expressions");
}

// Test that indexing a storage buffer array with a non-constant expression is forbidden, even if
// ANGLE is able to constant fold the index expression. ESSL 3.10 section 4.3.9.
TEST_P(GLSLValidationTest_ES31, DynamicallyIndexedStorageBuffer)
{
    constexpr char kFS[] = R"(#version 310 es
        precision mediump float;
        uniform int a;
        layout(std140) buffer B
        {
            vec4 f;
        }
        blocks[2];
        out vec4 my_FragColor;
        void main()
        {
            my_FragColor = blocks[true ? 0 : a].f;
        })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'[' : array indexes for shader storage block arrays must be constant integral "
                  "expressions");
}

// Test that indexing a sampler array with a non-constant expression is forbidden, even if ANGLE is
// able to constant fold the index expression. ESSL 3.00 section 4.1.7.1.
TEST_P(GLSLValidationTest_ES3, DynamicallyIndexedSampler)
{
    constexpr char kFS[] = R"(#version 300 es
        precision mediump float;
        uniform int a;
        uniform sampler2D s[2];
        out vec4 my_FragColor;
        void main()
        {
            my_FragColor = texture(s[true ? 0 : a], vec2(0));
        })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'[' : array index for samplers must be constant integral expressions");
}

// Test that indexing an image array with a non-constant expression is forbidden, even if ANGLE is
// able to constant fold the index expression. ESSL 3.10 section 4.1.7.2.
TEST_P(GLSLValidationTest_ES31, DynamicallyIndexedImage)
{
    constexpr char kFS[] = R"(#version 310 es
        precision mediump float;
        uniform int a;
        layout(rgba32f) uniform highp readonly image2D image[2];
        out vec4 my_FragColor;
        void main()
        {
            my_FragColor = imageLoad(image[true ? 0 : a], ivec2(0));
    })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  " '[' : array indexes for image arrays must be constant integral expressions");
}

// Test that a shader that uses a struct definition in place of a struct constructor does not
// compile. See GLSL ES 1.00 section 5.4.3.
TEST_P(GLSLValidationTest, StructConstructorWithStructDefinition)
{
    constexpr char kFS[] = R"(precision mediump float;
         void main() {
             struct s { float f; } (0.0);
             gl_FragColor = vec4(0.0);
    })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'structure' : constructor can't be a structure definition");
}

// Test that indexing gl_FragData with a non-constant expression is forbidden in WebGL 2.0, even
// when ANGLE is able to constant fold the index.
// WebGL 2.0 spec section 'GLSL ES 1.00 Fragment Shader Output'
TEST_P(WebGL2GLSLValidationTest, IndexFragDataWithNonConstant)
{

    constexpr char kFS[] = R"(precision mediump float;
         void main() {
             for (int i = 0; i < 2; ++i) {
                 gl_FragData[true ? 0 : i] = vec4(0.0);
             }
    })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'[' : array index for gl_FragData must be constant zero");
}

// Global variable initializers need to be constant expressions (ESSL 1.00 section 4.3)
// Initializing with an uniform should generate a warning
// (we don't generate an error on ESSL 1.00 because of WebGL compatibility)
TEST_P(WebGL2GLSLValidationTest, AssignUniformToGlobalESSL1)
{

    constexpr char kFS[] = R"(precision mediump float;
         uniform float a;
         float b = a * 2.0;
         void main() {
            gl_FragColor = vec4(b);
    })";

    validateWarning(GL_FRAGMENT_SHADER, kFS,
                    "'=' : global variable initializers should be constant expressions");
}

// Test that deferring global variable init works with an empty main().
TEST_P(WebGL2GLSLValidationTest, DeferGlobalVariableInitWithEmptyMain)
{
    constexpr char kFS[] = R"(precision mediump float;
         uniform float u;
         float foo = u;
         void main() {}
    )";

    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that a non-constant texture offset is not accepted for textureOffset.
// ESSL 3.00 section 8.8
TEST_P(GLSLValidationTest_ES3, TextureOffsetNonConst)
{
    constexpr char kFS[] = R"(#version 300 es
         precision mediump float;
         out vec4 my_FragColor;
         uniform vec3 u_texCoord;
         uniform mediump sampler3D u_sampler;
         uniform int x;
         void main() {
            my_FragColor = textureOffset(u_sampler, u_texCoord, ivec3(x, 3, -8));
    })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'textureOffset' : Texture offset must be a constant expression");
}

// Test that a non-constant texture offset is not accepted for textureProjOffset with bias.
// ESSL 3.00 section 8.8
TEST_P(GLSLValidationTest_ES3, TextureProjOffsetNonConst)
{
    constexpr char kFS[] = R"(#version 300 es
         precision mediump float;
         out vec4 my_FragColor;
         uniform vec4 u_texCoord;
         uniform mediump sampler3D u_sampler;
         uniform int x;
         void main() {
            my_FragColor = textureProjOffset(u_sampler, u_texCoord, ivec3(x, 3, -8), 0.0);
    })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'textureProjOffset' : Texture offset must be a constant expression");
}

// Test that an out-of-range texture offset is not accepted.
// GLES 3.0.4 section 3.8.10 specifies that out-of-range offset has undefined behavior.
TEST_P(GLSLValidationTest_ES3, TextureLodOffsetOutOfRange)
{

    GLint maxOffset = 0;

    glGetIntegerv(GL_MAX_PROGRAM_TEXEL_OFFSET, &maxOffset);

    const std::string kFS = R"(#version 300 es
    precision mediump float;
    out vec4 my_FragColor;
    uniform vec3 u_texCoord;
    uniform mediump sampler3D u_sampler;
    void main() {
        my_FragColor = textureLodOffset(u_sampler, u_texCoord, 0.0, ivec3(0, 0, )" +
                            std::to_string(maxOffset + 1) + R"());
    })";

    validateError(GL_FRAGMENT_SHADER, kFS.c_str(), "Texture offset value out of valid range");
}

// Test that default precision qualifier for uint is not accepted.
// ESSL 3.00.4 section 4.5.4: Only allowed for float, int and sampler types.
TEST_P(GLSLValidationTest_ES3, DefaultPrecisionUint)
{
    constexpr char kFS[] = R"(#version 300 es
         precision mediump float;
         precision mediump uint;
         out vec4 my_FragColor;
         void main() {
            my_FragColor = vec4(0.0);
    })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'uint' : illegal type argument for default precision qualifier");
}

// Test that sampler3D needs to be precision qualified.
// ESSL 3.00.4 section 4.5.4: New ESSL 3.00 sampler types don't have predefined precision.
TEST_P(GLSLValidationTest_ES3, NoPrecisionSampler3D)
{
    constexpr char kFS[] = R"(#version 300 es
         precision mediump float;
         uniform sampler3D s;
         out vec4 my_FragColor;
         void main() {
            my_FragColor = vec4(0.0);
    })";

    validateError(GL_FRAGMENT_SHADER, kFS, "'sampler3D' : No precision specified");
}

// Test that using a non-constant expression in a for loop initializer is forbidden in WebGL 1.0,
// even when ANGLE is able to constant fold the initializer.
// ESSL 1.00 Appendix A.
TEST_P(WebGLGLSLValidationTest, NonConstantLoopIndex)
{
    constexpr char kFS[] = R"(precision mediump float;
         uniform int u;
         void main() {
             for (int i = (true ? 1 : u); i < 5; ++i) {
                 gl_FragColor = vec4(0.0);
             }
    })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'i' : Loop index cannot be initialized with non-constant expression");
}

// Global variable initializers need to be constant expressions (ESSL 1.00 section 4.3)
// Initializing with an uniform should generate a warning
// (we don't generate an error on ESSL 1.00 because of WebGL compatibility)
TEST_P(WebGLGLSLValidationTest, AssignUniformToGlobalESSL1)
{
    constexpr char kFS[] = R"(precision mediump float;
         uniform float a;
         float b = a * 2.0;
         void main() {
            gl_FragColor = vec4(b);
    })";

    validateWarning(GL_FRAGMENT_SHADER, kFS,
                    "'=' : global variable initializers should be constant expressions");
}

// Test that deferring global variable init works with an empty main().
TEST_P(WebGLGLSLValidationTest, DeferGlobalVariableInitWithEmptyMain)
{
    constexpr char kFS[] = R"(precision mediump float;
         uniform float u;
         float foo = u;
         void main() {}
    )";

    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Check that indices that are not integers are rejected.
// The check should be done even if ESSL 1.00 Appendix A limitations are not applied.
TEST_P(GLSLValidationTest, NonIntegerIndex)
{
    constexpr char kFS[] = R"(precision mediump float;
         void main() {
             float f[3];
             const float i = 2.0;
             gl_FragColor = vec4(f[i]);
      })";

    validateError(GL_FRAGMENT_SHADER, kFS, "'[]' : integer expression required");
}

// ESSL1 shaders with a duplicate function prototype should be rejected.
// ESSL 1.00.17 section 4.2.7.
TEST_P(GLSLValidationTest, DuplicatePrototypeESSL1)
{
    constexpr char kFS[] = R"(precision mediump float;
         void foo();
         void foo();
         void foo() {}
         void main()
         {
             gl_FragColor = vec4(0.0);
    })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'function' : duplicate function prototype declarations are not allowed");
}

// ESSL3 shaders with a duplicate function prototype should be allowed.
// ESSL 3.00.4 section 4.2.3.
TEST_P(GLSLValidationTest_ES3, DuplicatePrototypeESSL3)
{
    constexpr char kFS[] = R"(#version 300 es
         precision mediump float;
         out vec4 my_FragColor;
         void foo();
         void foo();
         void foo() {}
         void main() {
             my_FragColor = vec4(0.0);
    })";

    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Shaders with a local function prototype should be rejected.
// ESSL 3.00.4 section 4.2.4.
TEST_P(GLSLValidationTest_ES3, LocalFunctionPrototype)
{
    constexpr char kFS[] = R"(#version 300 es
         precision mediump float;
         out vec4 my_FragColor;
         void main() {
             void foo();
             my_FragColor = vec4(0.0);
         })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  " 'function' : local function prototype declarations are not allowed");
}

// Built-in functions can not be overloaded in ESSL 3.00.
TEST_P(GLSLValidationTest_ES3, ESSL300BuiltInFunctionOverload)
{
    constexpr char kFS[] = R"(#version 300 es
         precision mediump float;
         out vec4 my_FragColor;
         int sin(int x) {
             return int(sin(float(x)));
         }
         void main() {
            my_FragColor = vec4(sin(1));
      })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'sin' : Name of a built-in function cannot be redeclared as function");
}

// Multiplying a 4x2 matrix with a 4x2 matrix should not work.
TEST_P(GLSLValidationTest_ES3, CompoundMultiplyMatrixIdenticalNonSquareDimensions)
{
    constexpr char kFS[] = R"(#version 300 es
         precision mediump float;
         out vec4 my_FragColor;
         void main() {
            mat4x2 foo;
            foo *= mat4x2(4.0);
            my_FragColor = vec4(0.0);
    })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'assign' : cannot convert from 'const 4X2 matrix of float' to 'mediump 4X2 "
                  "matrix of float'");
}

// ESSL 3.00 fragment shaders can not use #pragma STDGL invariant(all).
// ESSL 3.00.4 section 4.6.1. Does not apply to other versions of ESSL.
TEST_P(GLSLValidationTest_ES3, ESSL300FragmentInvariantAll)
{
    constexpr char kFS[] = R"(#version 300 es
         #pragma STDGL invariant(all)
         precision mediump float;
         out vec4 my_FragColor;
         void main() {
             my_FragColor = vec4(0.0);
         })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'invariant' : #pragma STDGL invariant(all) can not be used in fragment shader");
}

// Covers a bug where we would set the incorrect result size on an out-of-bounds vector swizzle.
TEST_P(GLSLValidationTest, OutOfBoundsVectorSwizzle)
{
    constexpr char kFS[] = R"(
        void main() {
            vec2(0).qq;
    })";

    validateError(GL_FRAGMENT_SHADER, kFS, "'qq' : vector field selection out of range");
}

// Covers a bug where strange preprocessor defines could trigger asserts.
TEST_P(GLSLValidationTest, DefineWithSemicolon)
{
    constexpr char kFS[] = R"(#define Def; highp
         uniform Def vec2 a;)";

    validateError(GL_FRAGMENT_SHADER, kFS, " '?' : Error during layout qualifier parsing.");
}

// Covers a bug in our parsing of malformed shift preprocessor expressions.
TEST_P(GLSLValidationTest, LineDirectiveUndefinedShift)
{
    constexpr char kFS[] = "#line x << y";

    validateError(GL_FRAGMENT_SHADER, kFS, "'x' : invalid line number");
}

// Covers a bug in our parsing of malformed shift preprocessor expressions.
TEST_P(GLSLValidationTest, LineDirectiveNegativeShift)
{
    constexpr char kFS[] = "#line x << -1";

    validateError(GL_FRAGMENT_SHADER, kFS, "'x' : invalid line number");
}

// gl_MaxImageUnits is only available in ES 3.1 shaders.
TEST_P(GLSLValidationTest_ES3, MaxImageUnitsInES3Shader)
{
    constexpr char kFS[] = R"(#version 300 es
         precision mediump float;
         out vec4 myOutput;
         void main() {
            float ff = float(gl_MaxImageUnits);
            myOutput = vec4(ff);
    })";

    validateError(GL_FRAGMENT_SHADER, kFS, "'gl_MaxImageUnits' : undeclared identifier");
}

// struct += struct is an invalid operation.
TEST_P(GLSLValidationTest_ES3, StructCompoundAssignStruct)
{
    constexpr char kFS[] = R"(#version 300 es
         precision mediump float;
         out vec4 myOutput;
         struct S { float foo; };
         void main() {
            S a, b;
            a += b;
            myOutput = vec4(0);
    })";

    validateError(GL_FRAGMENT_SHADER, kFS, "'+=' : Invalid operation for structs");
}

// struct == different struct is an invalid operation.
TEST_P(GLSLValidationTest_ES3, StructEqDifferentStruct)
{
    constexpr char kFS[] = R"(#version 300 es
         precision mediump float;
         out vec4 myOutput;
         struct S { float foo; };
         struct S2 { float foobar; };
         void main() {
            S a;
            S2 b;
            a == b;
            myOutput = vec4(0);
    })";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'==' : wrong operand types - no operation '==' exists that takes a left-hand "
                  "operand of type 'structure 'S'");
}

// Compute shaders are not supported in versions lower than 310.
TEST_P(GLSLValidationTest_ES31, Version100)
{
    constexpr char kCS[] = R"(void main()
        {
        })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "Compute shader is not supported in this shader version.");
}

// Compute shaders are not supported in versions lower than 310.
TEST_P(GLSLValidationTest_ES31, Version300)
{
    constexpr char kCS[] = R"(#version 300 es
        void main()
        {
        })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "Compute shader is not supported in this shader version.");
}

// Compute shaders should have work group size specified. However, it is not a compile time error
// to not have the size specified, but rather a link time one.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, NoWorkGroupSizeSpecified)
{
    constexpr char kCS[] = R"(#version 310 es
    void main() {
    })";

    validateSuccess(GL_COMPUTE_SHADER, kCS);
}

// Test that workgroup size declaration doesn't accept variable declaration.
TEST_P(GLSLValidationTest_ES31, NoVariableDeclrationAfterWorkGroupSize)
{
    constexpr char kCS[] = R"(#version 310 es
        layout(local_size_x = 1) in vec4 x;
        void main()
        {
        })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "'local_size_x' : invalid layout qualifier: only valid when used with 'in' in a "
                  "compute shader global layout declaration");
}

// Work group size is less than 1. It should be at least 1.
// GLSL ES 3.10 Revision 4, 7.1.3 Compute Shader Special Variables
// The spec is not clear whether having a local size qualifier equal zero
// is correct.
// TODO (mradev): Ask people from Khronos to clarify the spec.
TEST_P(GLSLValidationTest_ES31, WorkGroupSizeTooSmallXdimension)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 0) in;
    void main() {
    })";

    validateError(GL_COMPUTE_SHADER, kCS, "'0' : out of range: local_size_x must be positive");
}

// Work group size is correct for the x and y dimensions, but not for the z dimension.
// GLSL ES 3.10 Revision 4, 7.1.3 Compute Shader Special Variables
TEST_P(GLSLValidationTest_ES31, WorkGroupSizeTooSmallZDimension)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 4, local_size_y = 6, local_size_z = 0) in;
    void main() {
    })";

    validateError(GL_COMPUTE_SHADER, kCS, "'0' : out of range: local_size_z must be positive");
}

// Work group size is bigger than the maximum in the x dimension.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, WorkGroupSizeTooBigXDimension)
{

    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 9989899) in;
    void main() {
    })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "'local_size_x' : invalid value: Value must be at least 1 and no greater than");
}

// Work group size is bigger than the maximum in the y dimension.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, WorkGroupSizeTooBigYDimension)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 5, local_size_y = 9989899) in;
    void main() {
    })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "'local_size_y' : invalid value: Value must be at least 1 and no greater than");
}

// Work group size is definitely bigger than the maximum in the z dimension.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, WorkGroupSizeTooBigZDimension)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 5, local_size_y = 5, local_size_z = 9989899) in;
    void main() {
    })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "'local_size_z' : invalid value: Value must be at least 1 and no greater than");
}

// Work group size specified through macro expansion.
TEST_P(GLSLValidationTest_ES31, WorkGroupSizeMacro)
{
    constexpr char kCS[] = R"(#version 310 es
    #define MYDEF(x) x
    layout(local_size_x = MYDEF(127)) in;
    void main()
    {
    })";

    validateSuccess(GL_COMPUTE_SHADER, kCS);
}

// Work group size specified as an unsigned integer.
TEST_P(GLSLValidationTest_ES31, WorkGroupSizeUnsignedInteger)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 123u) in;
    void main() {
    })";

    validateSuccess(GL_COMPUTE_SHADER, kCS);
}

// Work group size specified in hexadecimal.
TEST_P(GLSLValidationTest_ES31, WorkGroupSizeHexadecimal)
{
    constexpr char kCS[] = R"(#version 310 es
         layout(local_size_x = 0x3A) in;
         void main()
         {
         })";

    validateSuccess(GL_COMPUTE_SHADER, kCS);
}

// local_size_x is -1 in hexadecimal format.
// -1 is used as unspecified value in the TLayoutQualifier structure.
TEST_P(GLSLValidationTest_ES31, WorkGroupSizeMinusOneHexadecimal)
{
    constexpr char kCS[] = R"(#version 310 es
         layout(local_size_x = 0xFFFFFFFF) in;
         void main()
         {
         })";

    validateError(GL_COMPUTE_SHADER, kCS, "'-1' : out of range: local_size_x must be positive");
}

// Work group size specified in octal.
TEST_P(GLSLValidationTest_ES31, WorkGroupSizeOctal)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 013) in;
    void main() {
    })";

    validateSuccess(GL_COMPUTE_SHADER, kCS);
}

// Work group size is negative. It is specified in hexadecimal.
TEST_P(GLSLValidationTest_ES31, WorkGroupSizeNegativeHexadecimal)
{
    constexpr char kCS[] = R"(#version 310 es
         layout(local_size_x = 0xFFFFFFEC) in;
         void main()
         {
         })";

    validateError(GL_COMPUTE_SHADER, kCS, "'-20' : out of range: local_size_x must be positive");
}

// Multiple work group layout qualifiers with differing values.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, DifferingLayoutQualifiers)
{
    constexpr char kCS[] = R"(#version 310 es
         layout(local_size_x = 5, local_size_x = 6) in;
         void main()
         {
         })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "'local_size_x' : Cannot have multiple different work group size specifiers");
}

// Multiple work group input variables with differing local size values.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, MultipleInputVariablesDifferingLocalSize)
{
    constexpr char kCS[] = R"(#version 310 es
         layout(local_size_x = 5, local_size_y = 6) in;
         layout(local_size_x = 5, local_size_y = 7) in;
         void main()
         {
         }
    )";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "'layout' : Work group size does not match the previous declaration");
}

// Multiple work group input variables with differing local size values.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, MultipleInputVariablesDifferingLocalSize2)
{
    constexpr char kCS[] = R"(#version 310 es
         layout(local_size_x = 5) in;
         layout(local_size_x = 5, local_size_y = 7) in;
         void main()
         {
         })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "'layout' : Work group size does not match the previous declaration");
}

// Multiple work group input variables with the same local size values. It should compile.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, MultipleInputVariablesSameLocalSize)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 5, local_size_y = 6) in;
    layout(local_size_x = 5, local_size_y = 6) in;
    void main() {
    })";

    validateSuccess(GL_COMPUTE_SHADER, kCS);
}

// Multiple work group input variables with the same local size values. It should compile.
// Since the default value is 1, it should compile.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, MultipleInputVariablesSameLocalSize2)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 5) in;
    layout(local_size_x = 5, local_size_y = 1) in;
    void main() {
    })";

    validateSuccess(GL_COMPUTE_SHADER, kCS);
}

// Multiple work group input variables with the same local size values. It should compile.
// Since the default value is 1, it should compile.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, MultipleInputVariablesSameLocalSize3)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 5, local_size_y = 1) in;
    layout(local_size_x = 5) in;
    void main() {
    })";

    validateSuccess(GL_COMPUTE_SHADER, kCS);
}

// Specifying row_major qualifier in a work group size layout.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, RowMajorInComputeInputLayout)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 5, row_major) in;
    void main()
    {
    })";

    validateError(GL_COMPUTE_SHADER, kCS, "'layout' : invalid layout qualifier combination");
}

// local size layout can be used only with compute input variables
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, UniformComputeInputLayout)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 5) uniform;
    void main() {
    })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "'local_size_x' : invalid layout qualifier: only valid when used with 'in' in a "
                  "compute shader global layout declaration");
}

// local size layout can be used only with compute input variables
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, UniformBufferComputeInputLayout)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 5) uniform SomeBuffer { vec4 something; };
    void main() {
    })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "'local_size_x' : invalid layout qualifier: only valid when used with 'in' in a "
                  "compute shader global layout declaration");
}

// local size layout can be used only with compute input variables
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, StructComputeInputLayout)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 5) struct SomeBuffer { vec4 something; };
    void main() {
    })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "'local_size_x' : invalid layout qualifier: only valid when used with 'in' in a "
                  "compute shader global layout declaration");
}

// local size layout can be used only with compute input variables
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, StructBodyComputeInputLayout)
{
    constexpr char kCS[] = R"(#version 310 es
    struct S {
      layout(local_size_x = 12) vec4 foo;
    };
    void main() {
    })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "'local_size_x' : invalid layout qualifier: only valid when used with 'in' in a "
                  "compute shader global layout declaration");
}

// local size layout can be used only with compute input variables
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, TypeComputeInputLayout)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 5) vec4;
    void main() {
    })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "'local_size_x' : invalid layout qualifier: only valid when used with 'in' in a "
                  "compute shader global layout declaration");
}

// Invalid use of the out storage qualifier in a compute shader.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, InvalidOutStorageQualifier)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 15) in;
    out vec4 myOutput;
    void main() {
    })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  " 'out' : storage qualifier isn't supported in compute shaders");
}

// Invalid use of the out storage qualifier in a compute shader.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, InvalidOutStorageQualifier2)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 15) in;
    out myOutput;
    void main() {
    })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "'out' : storage qualifier isn't supported in compute shaders");
}

// Invalid use of the in storage qualifier. Can be only used to describe the local block size.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, InvalidInStorageQualifier)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 15) in;
    in vec4 myInput;
    void main() {
    })";

    validateError(GL_COMPUTE_SHADER, kCS,
                  "'in' : 'in' can be only used to specify the local group size");
}

// Invalid use of the in storage qualifier. Can be only used to describe the local block size.
// The test checks a different part of the GLSL grammar than what InvalidInStorageQualifier
// checks.
// GLSL ES 3.10 Revision 4, 4.4.1.1 Compute Shader Inputs
TEST_P(GLSLValidationTest_ES31, InvalidInStorageQualifier2)
{
    constexpr char kCS[] = R"(#version 310 es
    layout(local_size_x = 15) in;
    in myInput;
    void main() {
    })";

    validateError(GL_COMPUTE_SHADER, kCS, "'myInput' : Expected invariant or precise");
}

// The local_size layout qualifier is only available in compute shaders.
TEST_P(GLSLValidationTest_ES31, VS_InvalidUseOfLocalSizeX)
{
    constexpr char kVS[] = R"(#version 310 es
    precision mediump float;
    layout(local_size_x = 15) in vec4 myInput;
    out vec4 myOutput;
    void main() {
        myOutput = myInput;
    })";

    validateError(GL_VERTEX_SHADER, kVS,
                  "'local_size_x' : invalid layout qualifier: only valid when used with 'in' in a "
                  "compute shader global layout declaration");
}

// The local_size layout qualifier is only available in compute shaders.
TEST_P(GLSLValidationTest_ES31, FS_InvalidUseOfLocalSizeX)
{
    constexpr char kFS[] = R"(#version 310 es
    precision mediump float;
    layout(local_size_x = 15) in vec4 myInput;
    out vec4 myOutput;
    void main() {
      myOutput = myInput;
    })";

    validateError(GL_VERTEX_SHADER, kFS,
                  "'local_size_x' : invalid layout qualifier: only valid when used with 'in' in a "
                  "compute shader global layout declaration");
}
// Verify that using maximum size as atomic counter offset results in compilation failure.
TEST_P(GLSLValidationTest_ES31, CompileWithMaxAtomicCounterOffsetFails)
{
    GLint maxSize;
    glGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE, &maxSize);

    std::ostringstream fs;
    fs << R"(#version 310 es
layout(location = 0) out uvec4 color;
layout(binding = 0, offset = )"
       << maxSize << R"() uniform atomic_uint a_counter;
void main() {
color = uvec4(atomicCounterIncrement(a_counter));
})";
    validateError(
        GL_FRAGMENT_SHADER, fs.str().c_str(),
        "'atomic counter' : Offset must not exceed the maximum atomic counter buffer size");
}

// Check that having an invalid char after the "." doesn't cause an assert.
TEST_P(GLSLValidationTest, InvalidFieldFirstChar)
{
    const char kVS[] = "void main() {vec4 x; x.}";
    validateError(GL_VERTEX_SHADER, kVS, ": '}' : Illegal character at fieldname start");
}

// Tests that bad index expressions don't crash ANGLE's translator.
// http://anglebug.com/42266998
TEST_P(GLSLValidationTest, BadIndexBugVec)
{
    constexpr char kFS[] = R"(precision mediump float;
uniform vec4 uniformVec;
void main()
{
    gl_FragColor = vec4(uniformVec[int()]);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'constructor' : constructor does not have any arguments");
}

// Tests that bad index expressions don't crash ANGLE's translator.
// http://anglebug.com/42266998
TEST_P(GLSLValidationTest, BadIndexBugMat)
{
    constexpr char kFS[] = R"(precision mediump float;
uniform mat4 uniformMat;
void main()
{
    gl_FragColor = vec4(uniformMat[int()]);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'constructor' : constructor does not have any arguments");
}

// Tests that bad index expressions don't crash ANGLE's translator.
// http://anglebug.com/42266998
TEST_P(GLSLValidationTest, BadIndexBugArray)
{
    constexpr char kFS[] = R"(precision mediump float;
uniform vec4 uniformArray;
void main()
{
    gl_FragColor = vec4(uniformArray[int()]);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'constructor' : constructor does not have any arguments");
}

// Test that GLSL error on gl_DepthRange does not crash.
TEST_P(GLSLValidationTestNoValidation, DepthRangeError)
{
    constexpr char kFS[] = R"(precision mediump float;
void main()
{
    gl_DepthRange + 1;
})";
    validateError(GL_FRAGMENT_SHADER, kFS, "'+' : Invalid operation for structs");
}

// Test that an inout value in a location beyond the MaxDrawBuffer limit when using the shader
// framebuffer fetch extension results in a compilation error.
// (Based on a fuzzer-discovered issue)
TEST_P(GLSLValidationTest_ES3, CompileFSWithInoutLocBeyondMaxDrawBuffers)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));

    GLint maxDrawBuffers;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);

    const std::string fs = R"(#version 300 es
#extension GL_EXT_shader_framebuffer_fetch : require
precision highp float;
layout(location = )" + std::to_string(maxDrawBuffers) +
                           R"() inout vec4 inoutArray[1];
void main()
{
    vec4 val = inoutArray[0];
    inoutArray[0] = val + vec4(0.1, 0.2, 0.3, 0.4);
})";
    validateError(GL_FRAGMENT_SHADER, fs.c_str(),
                  "'inoutArray' : output location must be < MAX_DRAW_BUFFERS");
}

// Test that structs with samplers are not allowed in interface blocks.  This is forbidden per
// GLES3:
//
// > Types and declarators are the same as for other uniform variable declarations outside blocks,
// > with these exceptions:
// > * opaque types are not allowed
TEST_P(GLSLValidationTest_ES3, StructWithSamplersDisallowedInInterfaceBlock)
{
    const char kFS[] = R"(#version 300 es
precision mediump float;
struct S { sampler2D samp; bool b; };

layout(std140) uniform Buffer { S s; } buffer;

out vec4 color;

void main()
{
    color = texture(buffer.s.samp, vec2(0));
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'Buffer' : Opaque types are not allowed in interface blocks");
}

// Test that *= on boolean vectors fails compilation
TEST_P(GLSLValidationTest, BVecMultiplyAssign)
{
    constexpr char kFS[] = R"(bvec4 c,s;void main(){s*=c;})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'assign' : cannot convert from '4-component vector of bool' to '4-component "
                  "vector of bool'");
}

// Test that packing of excessive 3-column variables does not overflow the count of 3-column
// variables in VariablePacker
TEST_P(WebGL2GLSLValidationTest, ExcessiveMat3UniformPacking)
{
    std::ostringstream vs;

    vs << R"(#version 300 es
precision mediump float;
out vec4 finalColor;
in vec4 color;
uniform mat4 r[254];

uniform mat3 )";

    constexpr size_t kNumUniforms = 10000;
    for (size_t i = 0; i < kNumUniforms; ++i)
    {
        if (i > 0)
        {
            vs << ", ";
        }
        vs << "m3a_" << i << "[256]";
    }
    vs << R"(;
void main(void) { finalColor = color; })";
    validateError(GL_VERTEX_SHADER, vs.str().c_str(), "too many uniforms");
}

// Test that infinite loop with while(true) is rejected
TEST_P(WebGL2GLSLValidationTest, InfiniteLoopWhileTrue)
{
    testInfiniteLoop(R"(#version 300 es
precision highp float;
uniform uint zero;
out vec4 color;

void main()
{
    float r = 0.;
    float g = 1.;
    float b = 0.;

    // Infinite loop
    while (true)
    {
        r += 0.1;
        if (r > 0.)
        {
            continue;
        }
    }

    color = vec4(r, g, b, 1);
})");
}

// Test that infinite loop with for(;true;) is rejected
TEST_P(WebGL2GLSLValidationTest, InfiniteLoopForTrue)
{
    testInfiniteLoop(R"(#version 300 es
precision highp float;
uniform uint zero;
out vec4 color;

void main()
{
    float r = 0.;
    float g = 1.;
    float b = 0.;

    // Infinite loop
    for (;!false;)
    {
        r += 0.1;
    }

    color = vec4(r, g, b, 1);
})");
}

// Test that infinite loop with do{} while(true) is rejected
TEST_P(WebGL2GLSLValidationTest, InfiniteLoopDoWhileTrue)
{
    testInfiniteLoop(R"(#version 300 es
precision highp float;
uniform uint zero;
out vec4 color;

void main()
{
    float r = 0.;
    float g = 1.;
    float b = 0.;

    // Infinite loop
    do
    {
        r += 0.1;
        switch (uint(r))
        {
            case 0:
                g += 0.1;
                break;
            default:
                b += 0.1;
                continue;
        }
    } while (true);

    color = vec4(r, g, b, 1);
})");
}

// Test that infinite loop with constant local variable is rejected
TEST_P(WebGL2GLSLValidationTest, InfiniteLoopLocalVariable)
{
    testInfiniteLoop(R"(#version 300 es
precision highp float;
uniform uint zero;
out vec4 color;

void main()
{
    float r = 0.;
    float g = 1.;
    float b = 0.;

    bool localConstTrue = true;

    // Infinite loop
    do
    {
        r += 0.1;
        switch (uint(r))
        {
            case 0:
                g += 0.1;
                break;
            default:
                b += 0.1;
                continue;
        }
    } while (localConstTrue);

    color = vec4(r, g, b, 1);
})");
}

// Test that infinite loop with global variable is rejected
TEST_P(WebGL2GLSLValidationTest, InfiniteLoopGlobalVariable)
{
    testInfiniteLoop(R"(#version 300 es
precision highp float;
uniform uint zero;
out vec4 color;

bool globalConstTrue = true;

void main()
{
    float r = 0.;
    float g = 1.;
    float b = 0.;

    // Infinite loop
    do
    {
        r += 0.1;
        switch (uint(r))
        {
            case 0:
                g += 0.1;
                break;
            default:
                b += 0.1;
                continue;
        }
    } while (globalConstTrue);

    color = vec4(r, g, b, 1);
})");
}

// Test that indexing swizzles out of bounds fails
TEST_P(GLSLValidationTest_ES3, OutOfBoundsIndexingOfSwizzle)
{
    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
out vec4 colorOut;
uniform vec3 colorIn;

void main()
{
    colorOut = vec4(colorIn.yx[2], 0, 0, 1);
})";
    validateError(GL_FRAGMENT_SHADER, kFS, "'[]' : vector field selection out of range");
}

// Regression test for a validation bug in the translator where func(void, int) was accepted even
// though it's illegal, and the function was callable as if the void parameter isn't there.
TEST_P(GLSLValidationTest, NoParameterAfterVoid)
{
    constexpr char kVS[] = R"(void f(void, int a){}
void main(){f(1);})";
    validateError(GL_VERTEX_SHADER, kVS, "'void' : cannot be a parameter type except for '(void)'");
}

// Similar to NoParameterAfterVoid, but tests func(void, void).
TEST_P(GLSLValidationTest, NoParameterAfterVoid2)
{
    constexpr char kVS[] = R"(void f(void, void){}
void main(){f();})";
    validateError(GL_VERTEX_SHADER, kVS, "'void' : cannot be a parameter type except for '(void)'");
}

// Test that structs with too many fields are rejected.  In SPIR-V, the instruction that defines the
// struct lists the fields which means the length of the instruction is a function of the field
// count.  Since SPIR-V instruction sizes are limited to 16 bits, structs with more fields cannot be
// represented.
TEST_P(GLSLValidationTest_ES3, TooManyFieldsInStruct)
{
    std::ostringstream fs;
    fs << R"(#version 300 es
precision highp float;
struct TooManyFields
{
)";
    for (uint32_t i = 0; i < (1 << 16); ++i)
    {
        fs << "    float field" << i << ";\n";
    }
    fs << R"(};
uniform B { TooManyFields s; };
out vec4 color;
void main() {
    color = vec4(s.field0, 0.0, 0.0, 1.0);
})";
    validateError(GL_FRAGMENT_SHADER, fs.str().c_str(),
                  "'TooManyFields' : Too many fields in the struct");
}

// Same as TooManyFieldsInStruct, but with samplers in the struct.
TEST_P(GLSLValidationTest_ES3, TooManySamplerFieldsInStruct)
{
    std::ostringstream fs;
    fs << R"(#version 300 es
precision highp float;
struct TooManyFields
{
)";
    for (uint32_t i = 0; i < (1 << 16); ++i)
    {
        fs << "    sampler2D field" << i << ";\n";
    }
    fs << R"(};
uniform TooManyFields s;
out vec4 color;
void main() {
    color = texture(s.field0, vec2(0));
})";
    validateError(GL_FRAGMENT_SHADER, fs.str().c_str(),
                  "'TooManyFields' : Too many fields in the struct");
}

// Test having many samplers in nested structs.
TEST_P(GLSLValidationTest_ES3, ManySamplerFieldsInStructComplex)
{
    // D3D and OpenGL may be more restrictive about this many samplers.
    ANGLE_SKIP_TEST_IF(IsD3D() || IsOpenGL());

    const char kFS[] = R"(#version 300 es
precision highp float;

struct X {
    mediump sampler2D a[0xf00];
    mediump sampler2D b[0xf00];
    mediump sampler2D c[0xf000];
    mediump sampler2D d[0xf00];
};

struct Y {
  X s1;
  mediump sampler2D a[0xf00];
  mediump sampler2D b[0xf000];
  mediump sampler2D c[0x14000];
};

struct S {
    Y s1;
};

struct structBuffer { S s; };

uniform structBuffer b;

out vec4 color;
void main()
{
    color = texture(b.s.s1.s1.c[0], vec2(0));
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Make sure a large array of samplers works.
TEST_P(GLSLValidationTest, ManySamplers)
{
    // D3D and OpenGL may be more restrictive about this many samplers.
    ANGLE_SKIP_TEST_IF(IsD3D() || IsOpenGL());

    const char kFS[] = R"(precision highp float;

uniform mediump sampler2D c[0x12000];

void main()
{
    gl_FragColor = texture2D(c[0], vec2(0));
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Make sure a large array of samplers works when declared in a struct.
TEST_P(GLSLValidationTest, ManySamplersInStruct)
{
    // D3D and OpenGL may be more restrictive about this many samplers.
    ANGLE_SKIP_TEST_IF(IsD3D() || IsOpenGL());

    const char kFS[] = R"(precision highp float;

struct X {
    mediump sampler2D c[0x12000];
};

uniform X x;

void main()
{
    gl_FragColor = texture2D(x.c[0], vec2(0));
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that passing large arrays to functions are compiled correctly.  Regression test for the
// SPIR-V generator that made a copy of the array to pass to the function, by decomposing and
// reconstructing it (in the absence of OpCopyLogical), but the reconstruction instruction has a
// length higher than can fit in SPIR-V.
TEST_P(GLSLValidationTest_ES3, LargeInterfaceBlockArrayPassedToFunction)
{
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
uniform Large { float a[65536]; };
float f(float b[65536])
{
    b[0] = 1.0;
    return b[0] + b[1];
}
out vec4 color;
void main() {
    color = vec4(f(a), 0.0, 0.0, 1.0);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "Size of declared private variable exceeds implementation-defined limit");
}

// Similar to LargeInterfaceBlockArrayPassedToFunction, but the array is nested in a struct.
TEST_P(GLSLValidationTest_ES3, LargeInterfaceBlockNestedArrayPassedToFunction)
{
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
struct S { float a[65536]; };
uniform Large { S s; };
float f(float b[65536])
{
    b[0] = 1.0;
    return b[0] + b[1];
}
out vec4 color;
void main() {
    color = vec4(f(s.a), 0.0, 0.0, 1.0);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "Size of declared private variable exceeds implementation-defined limit");
}

// Similar to LargeInterfaceBlockArrayPassedToFunction, but the large array is copied to a local
// variable instead.
TEST_P(GLSLValidationTest_ES3, LargeInterfaceBlockArrayCopiedToLocal)
{
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
uniform Large { float a[65536]; };
out vec4 color;
void main() {
    float b[65536] = a;
    color = vec4(b[0], 0.0, 0.0, 1.0);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "Size of declared private variable exceeds implementation-defined limit");
}

// Similar to LargeInterfaceBlockArrayCopiedToLocal, but the array is nested in a struct
TEST_P(GLSLValidationTest_ES3, LargeInterfaceBlockNestedArrayCopiedToLocal)
{
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
struct S { float a[65536]; };
uniform Large { S s; };
out vec4 color;
void main() {
    S s2 = s;
    color = vec4(s2.a[0], 0.0, 0.0, 1.0);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "Size of declared private variable exceeds implementation-defined limit");
}

// Test that too large varyings are rejected.
TEST_P(GLSLValidationTest_ES3, LargeArrayVarying)
{
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
in float a[65536];
out vec4 color;
void main() {
    color = vec4(a[0], 0.0, 0.0, 1.0);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'a' : Size of declared private variable exceeds implementation-defined limit");
}

// Test that too large color outputs are rejected
TEST_P(GLSLValidationTest_ES3, LargeColorOutput)
{
    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    ANGLE_SKIP_TEST_IF(maxDrawBuffers >= 32);

    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
out vec4 colorOut[32];

void main()
{
    colorOut[0] = vec4(0, 0, 0, 1);
    colorOut[31] = vec4(0, 0, 0, 1);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'colorOut' : output array locations would exceed MAX_DRAW_BUFFERS");
}

// Test that too large color outputs are rejected
TEST_P(GLSLValidationTest_ES3, LargeColorOutputWithLocation)
{
    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    ANGLE_SKIP_TEST_IF(maxDrawBuffers >= 10);

    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
layout(location = 0) out vec4 colorOut[4];
layout(location = 4) out vec4 colorOut2[6];

void main()
{
    colorOut[0] = vec4(0, 0, 0, 1);
    colorOut[3] = vec4(0, 0, 0, 1);
    colorOut2[0] = vec4(0, 0, 0, 1);
    colorOut2[5] = vec4(0, 0, 0, 1);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'colorOut2' : output array locations would exceed MAX_DRAW_BUFFERS");
}

// Test that marking a built-in as invariant and then redeclaring it is an error.
TEST_P(GLSLValidationTest_ES3, FragDepthInvariantThenRedeclare)
{
    constexpr char kFS[] = R"(#version 300 es
#extension GL_EXT_conservative_depth:enable
precision mediump float;
invariant gl_FragDepth;
out float gl_FragDepth;
void main() {
    gl_FragDepth = 0.5;
}
)";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'gl_FragDepth' : built-ins cannot be redeclared after being qualified as "
                  "invariant or precise");
}

// Make sure gl_PerVertex is not accepted other than as `out` and with no name in vertex shader
TEST_P(GLSLValidationTest_ES31, ValidatePerVertexVertexShader)
{
    {
        // Cannot use gl_PerVertex with attribute
        constexpr char kVS[] = R"(attribute gl_PerVertex{vec4 gl_Position;};
void main() {})";
        validateError(GL_VERTEX_SHADER, kVS,
                      "'gl_PerVertex' : interface blocks supported in GLSL ES 3.00 and above only");
    }

    {
        // Cannot use gl_PerVertex with a name (without EXT_shader_io_blocks)
        constexpr char kVS[] = R"(#version 300 es
out gl_PerVertex{vec4 gl_Position;} name;
void main() {})";
        validateError(GL_VERTEX_SHADER, kVS,
                      "'out' : invalid qualifier: interface blocks must be uniform in version "
                      "lower than GLSL ES 3.10");
    }

    {
        // Cannot use gl_PerVertex (without EXT_shader_io_blocks)
        constexpr char kVS[] = R"(#version 310 es
out gl_PerVertex{vec4 gl_Position;};
void main() {})";
        validateError(GL_VERTEX_SHADER, kVS,
                      "'out' : invalid qualifier: shader IO blocks need shader io block extension");
    }

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_io_blocks"));

    {
        // Cannot use gl_PerVertex with a name
        constexpr char kVS[] = R"(#version 310 es
#extension GL_EXT_shader_io_blocks : require
out gl_PerVertex{vec4 gl_Position;} name;
void main() {})";
        validateError(GL_VERTEX_SHADER, kVS,
                      "'name' : out gl_PerVertex instance name must be empty in this shader");
    }

    {
        // out gl_PerVertex without a name is ok.
        constexpr char kVS[] = R"(#version 310 es
#extension GL_EXT_shader_io_blocks : require
out gl_PerVertex{vec4 gl_Position;};
void main() {})";
        validateSuccess(GL_VERTEX_SHADER, kVS);
    }
}

// Make sure gl_PerVertex is not accepted other than as `out .. gl_out[]`, or `in .. gl_in[]` in
// tessellation control shader.
TEST_P(GLSLValidationTest_ES31, ValidatePerVertexTessellationControlShader)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_tessellation_shader"));

    {
        // Cannot use out gl_PerVertex with a name (without EXT_shader_io_blocks)
        constexpr char kTCS[] = R"(#version 300 es
out gl_PerVertex{vec4 gl_Position;} name[];
void main() {})";
        validateError(GL_TESS_CONTROL_SHADER, kTCS,
                      "'out' : invalid qualifier: interface blocks must be uniform in version "
                      "lower than GLSL ES 3.10");
    }

    {
        // Cannot use in gl_PerVertex with a name (without EXT_shader_io_blocks)
        constexpr char kTCS[] = R"(#version 300 es
in gl_PerVertex{vec4 gl_Position;} name[];
void main() {})";
        validateError(GL_TESS_CONTROL_SHADER, kTCS,
                      "'in' : invalid qualifier: interface blocks must be uniform in version lower "
                      "than GLSL ES 3.10");
    }

    {
        // Cannot use out gl_PerVertex (without EXT_shader_io_blocks)
        constexpr char kTCS[] = R"(#version 310 es
out gl_PerVertex{vec4 gl_Position;} gl_out[];
void main() {})";
        validateError(GL_TESS_CONTROL_SHADER, kTCS,
                      "'out' : invalid qualifier: shader IO blocks need shader io block extension");
    }

    {
        // Cannot use in gl_PerVertex (without EXT_shader_io_blocks)
        constexpr char kTCS[] = R"(#version 310 es
in gl_PerVertex{vec4 gl_Position;} gl_in[];
void main() {})";
        validateError(GL_TESS_CONTROL_SHADER, kTCS,
                      "'in' : invalid qualifier: shader IO blocks need shader io block extension");
    }

    {
        // Cannot use out gl_PerVertex with a name
        constexpr char kTCS[] = R"(#version 310 es
#extension GL_EXT_tessellation_shader : require
layout (vertices=4) out;
out gl_PerVertex{vec4 gl_Position;} name[];
void main() {})";
        validateError(GL_TESS_CONTROL_SHADER, kTCS,
                      "'name' : out gl_PerVertex instance name must be gl_out in this shader");
    }

    {
        // Cannot use in gl_PerVertex with a name
        constexpr char kTCS[] = R"(#version 310 es
#extension GL_EXT_tessellation_shader : require
layout (vertices=4) out;
in gl_PerVertex{vec4 gl_Position;} name[];
void main() {})";
        validateError(GL_TESS_CONTROL_SHADER, kTCS,
                      "'name' : in gl_PerVertex instance name must be gl_in");
    }

    {
        // Cannot use out gl_PerVertex if not arrayed
        constexpr char kTCS[] = R"(#version 310 es
#extension GL_EXT_tessellation_shader : require
layout (vertices=4) out;
out gl_PerVertex{vec4 gl_Position;} gl_out;
void main() {})";
        validateError(GL_TESS_CONTROL_SHADER, kTCS, "'gl_PerVertex' : type must be an array");
    }

    {
        // Cannot use in gl_PerVertex if not arrayed
        constexpr char kTCS[] = R"(#version 310 es
#extension GL_EXT_tessellation_shader : require
layout (vertices=4) out;
in gl_PerVertex{vec4 gl_Position;} gl_in;
void main() {})";
        validateError(GL_TESS_CONTROL_SHADER, kTCS, "'gl_PerVertex' : type must be an array");
    }

    {
        // out gl_PerVertex with gl_out, and in gl_PerVertex with gl_in are ok.
        constexpr char kTCS[] = R"(#version 310 es
#extension GL_EXT_tessellation_shader : require
layout (vertices=4) out;
out gl_PerVertex{vec4 gl_Position;} gl_out[];
in gl_PerVertex{vec4 gl_Position;} gl_in[];
void main() {})";
        validateSuccess(GL_TESS_CONTROL_SHADER, kTCS);
    }
}

// Make sure gl_PerVertex is not accepted other than as `out .. gl_out`, or `in .. gl_in[]` in
// tessellation evaluation shader.
TEST_P(GLSLValidationTest_ES31, ValidatePerVertexTessellationEvaluationShader)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_tessellation_shader"));

    {
        // Cannot use out gl_PerVertex with a name (without EXT_shader_io_blocks)
        constexpr char kTES[] = R"(#version 300 es
out gl_PerVertex{vec4 gl_Position;} name;
void main() {})";
        validateError(GL_TESS_EVALUATION_SHADER, kTES,
                      "'out' : invalid qualifier: interface blocks must be uniform in version "
                      "lower than GLSL ES 3.10");
    }

    {
        // Cannot use in gl_PerVertex with a name (without EXT_shader_io_blocks)
        constexpr char kTES[] = R"(#version 300 es
in gl_PerVertex{vec4 gl_Position;} name[];
void main() {})";
        validateError(GL_TESS_EVALUATION_SHADER, kTES,
                      "'in' : invalid qualifier: interface blocks must be uniform in version lower "
                      "than GLSL ES 3.10");
    }

    {
        // Cannot use out gl_PerVertex (without EXT_shader_io_blocks)
        constexpr char kTES[] = R"(#version 310 es
out gl_PerVertex{vec4 gl_Position;};
void main() {})";
        validateError(GL_TESS_EVALUATION_SHADER, kTES,
                      "'out' : invalid qualifier: shader IO blocks need shader io block extension");
    }

    {
        // Cannot use in gl_PerVertex (without EXT_shader_io_blocks)
        constexpr char kTES[] = R"(#version 310 es
in gl_PerVertex{vec4 gl_Position;} gl_in[];
void main() {})";
        validateError(GL_TESS_EVALUATION_SHADER, kTES,
                      "'in' : invalid qualifier: shader IO blocks need shader io block extension");
    }

    {
        // Cannot use out gl_PerVertex with a name
        constexpr char kTES[] = R"(#version 310 es
#extension GL_EXT_tessellation_shader : require
layout (isolines, point_mode) in;
out gl_PerVertex{vec4 gl_Position;} name;
void main() {})";
        validateError(GL_TESS_EVALUATION_SHADER, kTES,
                      "'name' : out gl_PerVertex instance name must be empty in this shader");
    }

    {
        // Cannot use in gl_PerVertex with a name
        constexpr char kTES[] = R"(#version 310 es
#extension GL_EXT_tessellation_shader : require
layout (isolines, point_mode) in;
in gl_PerVertex{vec4 gl_Position;} name[];
void main() {})";
        validateError(GL_TESS_EVALUATION_SHADER, kTES,
                      "'name' : in gl_PerVertex instance name must be gl_in");
    }

    {
        // Cannot use out gl_PerVertex if arrayed
        constexpr char kTES[] = R"(#version 310 es
#extension GL_EXT_tessellation_shader : require
layout (isolines, point_mode) in;
out gl_PerVertex{vec4 gl_Position;} gl_out[];
void main() {})";
        validateError(GL_TESS_EVALUATION_SHADER, kTES,
                      "'gl_out' : out gl_PerVertex instance name must be empty in this shader");
    }

    {
        // Cannot use in gl_PerVertex if not arrayed
        constexpr char kTES[] = R"(#version 310 es
#extension GL_EXT_tessellation_shader : require
layout (isolines, point_mode) in;
in gl_PerVertex{vec4 gl_Position;} gl_in;
void main() {})";
        validateError(GL_TESS_EVALUATION_SHADER, kTES, "'gl_PerVertex' : type must be an array");
    }

    {
        // out gl_PerVertex without a name, and in gl_PerVertex with gl_in are ok.
        constexpr char kTES[] = R"(#version 310 es
#extension GL_EXT_tessellation_shader : require
layout (isolines, point_mode) in;
out gl_PerVertex{vec4 gl_Position;};
in gl_PerVertex{vec4 gl_Position;} gl_in[];
void main() {})";
        validateSuccess(GL_TESS_EVALUATION_SHADER, kTES);
    }
}

// Make sure gl_PerVertex is not accepted other than as `out .. gl_out`, or `in .. gl_in[]` in
// geometry shader.
TEST_P(GLSLValidationTest_ES31, ValidatePerVertexGeometryShader)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    {
        // Cannot use out gl_PerVertex with a name (without EXT_shader_io_blocks)
        constexpr char kGS[] = R"(#version 300 es
out gl_PerVertex{vec4 gl_Position;} name;
void main() {})";
        validateError(GL_GEOMETRY_SHADER, kGS,
                      "'out' : invalid qualifier: interface blocks must be uniform in version "
                      "lower than GLSL ES 3.10");
    }

    {
        // Cannot use in gl_PerVertex with a name (without EXT_shader_io_blocks)
        constexpr char kGS[] = R"(#version 300 es
in gl_PerVertex{vec4 gl_Position;} name[];
void main() {})";
        validateError(GL_GEOMETRY_SHADER, kGS,
                      "'in' : invalid qualifier: interface blocks must be uniform in version lower "
                      "than GLSL ES 3.10");
    }

    {
        // Cannot use out gl_PerVertex (without EXT_shader_io_blocks)
        constexpr char kGS[] = R"(#version 310 es
out gl_PerVertex{vec4 gl_Position;};
void main() {})";
        validateError(GL_GEOMETRY_SHADER, kGS,
                      "'out' : invalid qualifier: shader IO blocks need shader io block extension");
    }

    {
        // Cannot use in gl_PerVertex (without EXT_shader_io_blocks)
        constexpr char kGS[] = R"(#version 310 es
in gl_PerVertex{vec4 gl_Position;} gl_in[];
void main() {})";
        validateError(GL_GEOMETRY_SHADER, kGS,
                      "'in' : invalid qualifier: shader IO blocks need shader io block extension");
    }

    {
        // Cannot use out gl_PerVertex with a name
        constexpr char kGS[] = R"(#version 310 es
#extension GL_EXT_geometry_shader : require
layout (triangles) in;
layout (points, max_vertices = 1) out;
out gl_PerVertex{vec4 gl_Position;} name;
void main() {})";
        validateError(GL_GEOMETRY_SHADER, kGS,
                      "'name' : out gl_PerVertex instance name must be empty in this shader");
    }

    {
        // Cannot use in gl_PerVertex with a name
        constexpr char kGS[] = R"(#version 310 es
#extension GL_EXT_geometry_shader : require
layout (triangles) in;
layout (points, max_vertices = 1) out;
in gl_PerVertex{vec4 gl_Position;} name[];
void main() {})";
        validateError(GL_GEOMETRY_SHADER, kGS,
                      "'name' : in gl_PerVertex instance name must be gl_in");
    }

    {
        // Cannot use out gl_PerVertex if arrayed
        constexpr char kGS[] = R"(#version 310 es
#extension GL_EXT_geometry_shader : require
layout (triangles) in;
layout (points, max_vertices = 1) out;
out gl_PerVertex{vec4 gl_Position;} gl_out[];
void main() {})";
        validateError(GL_GEOMETRY_SHADER, kGS,
                      "'gl_out' : out gl_PerVertex instance name must be empty in this shader");
    }

    {
        // Cannot use in gl_PerVertex if not arrayed
        constexpr char kGS[] = R"(#version 310 es
#extension GL_EXT_geometry_shader : require
layout (triangles) in;
layout (points, max_vertices = 1) out;
in gl_PerVertex{vec4 gl_Position;} gl_in;
void main() {})";
        validateError(GL_GEOMETRY_SHADER, kGS, "'gl_PerVertex' : type must be an array");
    }

    {
        // out gl_PerVertex without a name, and in gl_PerVertex with gl_in are ok.
        constexpr char kGS[] = R"(#version 310 es
#extension GL_EXT_geometry_shader : require
layout (triangles) in;
layout (points, max_vertices = 1) out;
out gl_PerVertex{vec4 gl_Position;};
in gl_PerVertex{vec4 gl_Position;} gl_in[];
void main() {})";
        validateSuccess(GL_GEOMETRY_SHADER, kGS);
    }
}

// Regression test case of unary + constant folding of a void struct member.
TEST_P(GLSLValidationTest, UnaryPlusOnVoidStructMemory)
{
    constexpr char kFS[] = R"(uniform mediump vec4 u;
struct U
{
    void t;
};
void main() {
  +U().t;
})";
    validateError(GL_FRAGMENT_SHADER, kFS, "'t' : illegal use of type 'void'");
}

// Test compiling shaders using the GL_EXT_shader_texture_lod extension
TEST_P(GLSLValidationTest, TextureLOD)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_texture_lod"));

    constexpr char kFS[] = R"(#extension GL_EXT_shader_texture_lod : require
uniform sampler2D u_texture;
void main() {
    gl_FragColor = texture2DGradEXT(u_texture, vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0.0, 0.0));
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Verify that using a struct as both invariant and non-invariant output works.
TEST_P(GLSLValidationTest_ES31, StructBothInvariantAndNot)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_io_blocks"));

    constexpr char kVS[] = R"(#version 310 es
#extension GL_EXT_shader_io_blocks : require

struct S
{
    vec4 s;
};

out Output
{
    vec4 x;
    invariant S s;
};

out S s2;

void main(){
    x = vec4(0);
    s.s = vec4(1);
    s2.s = vec4(2);
    S s3 = s;
    s.s = s3.s;
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Verify that using a struct as both invariant and non-invariant output works.
// The shader interface block has a variable name in this variant.
TEST_P(GLSLValidationTest_ES31, StructBothInvariantAndNot2)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_io_blocks"));

    constexpr char kVS[] = R"(#version 310 es
#extension GL_EXT_shader_io_blocks : require

struct S
{
    vec4 s;
};

out Output
{
    vec4 x;
    invariant S s;
} o;

out S s2;

void main(){
    o.x = vec4(0);
    o.s.s = vec4(1);
    s2.s = vec4(2);
    S s3 = o.s;
    o.s.s = s3.s;
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Verify that functions without return statements still compile
TEST_P(GLSLValidationTest, MissingReturnFloat)
{
    constexpr char kVS[] = R"(varying float v_varying;
float f();
void main() { gl_Position = vec4(f(), 0, 0, 1); }
float f() { if (v_varying > 0.0) return 1.0; })";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Verify that functions without return statements still compile
TEST_P(GLSLValidationTest, MissingReturnVec2)
{
    constexpr char kVS[] = R"(varying float v_varying;
vec2 f() { if (v_varying > 0.0) return vec2(1.0, 1.0); }
void main() { gl_Position = vec4(f().x, 0, 0, 1); })";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Verify that functions without return statements still compile
TEST_P(GLSLValidationTest, MissingReturnVec3)
{
    constexpr char kVS[] = R"(varying float v_varying;
vec3 f() { if (v_varying > 0.0) return vec3(1.0, 1.0, 1.0); }
void main() { gl_Position = vec4(f().x, 0, 0, 1); })";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Verify that functions without return statements still compile
TEST_P(GLSLValidationTest, MissingReturnVec4)
{
    constexpr char kVS[] = R"(varying float v_varying;
vec4 f() { if (v_varying > 0.0) return vec4(1.0, 1.0, 1.0, 1.0); }
void main() { gl_Position = vec4(f().x, 0, 0, 1); })";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Verify that functions without return statements still compile
TEST_P(GLSLValidationTest, MissingReturnIVec4)
{
    constexpr char kVS[] = R"(varying float v_varying;
ivec4 f() { if (v_varying > 0.0) return ivec4(1, 1, 1, 1); }
void main() { gl_Position = vec4(f().x, 0, 0, 1); })";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Verify that functions without return statements still compile
TEST_P(GLSLValidationTest, MissingReturnMat4)
{
    constexpr char kVS[] = R"(varying float v_varying;
mat4 f() { if (v_varying > 0.0) return mat4(1.0); }
void main() { gl_Position = vec4(f()[0][0], 0, 0, 1); })";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Verify that functions without return statements still compile
TEST_P(GLSLValidationTest, MissingReturnStruct)
{
    constexpr char kVS[] = R"(varying float v_varying;
struct s { float a; int b; vec2 c; };
s f() { if (v_varying > 0.0) return s(1.0, 1, vec2(1.0, 1.0)); }
void main() { gl_Position = vec4(f().a, 0, 0, 1); })";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Verify that functions without return statements still compile
TEST_P(GLSLValidationTest_ES3, MissingReturnArray)
{
    constexpr char kVS[] = R"(#version 300 es
in float v_varying;
vec2[2] f() { if (v_varying > 0.0) { return vec2[2](vec2(1.0, 1.0), vec2(1.0, 1.0)); } }
void main() { gl_Position = vec4(f()[0].x, 0, 0, 1); })";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Verify that functions without return statements still compile
TEST_P(GLSLValidationTest_ES3, MissingReturnArrayOfStructs)
{
    constexpr char kVS[] = R"(#version 300 es
in float v_varying;
struct s { float a; int b; vec2 c; };
s[2] f() { if (v_varying > 0.0) { return s[2](s(1.0, 1, vec2(1.0, 1.0)), s(1.0, 1, vec2(1.0, 1.0))); } }
void main() { gl_Position = vec4(f()[0].a, 0, 0, 1); })";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Verify that functions without return statements still compile
TEST_P(GLSLValidationTest_ES3, MissingReturnStructOfArrays)
{
    // TODO(crbug.com/998505): Test failing on Android FYI Release (NVIDIA Shield TV)
    ANGLE_SKIP_TEST_IF(IsNVIDIAShield());

    constexpr char kVS[] = R"(#version 300 es
in float v_varying;
struct s { float a[2]; int b[2]; vec2 c[2]; };
s f() { if (v_varying > 0.0) { return s(float[2](1.0, 1.0), int[2](1, 1), vec2[2](vec2(1.0, 1.0), vec2(1.0, 1.0))); } }
void main() { gl_Position = vec4(f().a[0], 0, 0, 1); })";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Verify that non-const index used on an array returned by a function compiles
TEST_P(GLSLValidationTest_ES3, ReturnArrayOfStructsThenNonConstIndex)
{
    constexpr char kVS[] = R"(#version 300 es
in float v_varying;
struct s { float a; int b; vec2 c; };
s[2] f()
{
    return s[2](s(v_varying, 1, vec2(1.0, 1.0)), s(v_varying / 2.0, 1, vec2(1.0, 1.0)));
}
void main()
{
    gl_Position = vec4(f()[uint(v_varying)].a, 0, 0, 1);
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Verify shader source with a fixed length that is less than the null-terminated length will
// compile.
TEST_P(GLSLValidationTest, FixedShaderLength)
{
    GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);

    const std::string appendGarbage = "abcdefghijklmnopqrstuvwxyz";
    const std::string source   = "void main() { gl_FragColor = vec4(0, 0, 0, 0); }" + appendGarbage;
    const char *sourceArray[1] = {source.c_str()};
    GLint lengths[1]           = {static_cast<GLint>(source.length() - appendGarbage.length())};
    glShaderSource(shader, static_cast<GLsizei>(ArraySize(sourceArray)), sourceArray, lengths);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);
    EXPECT_NE(compileResult, 0);
}

// Verify that a negative shader source length is treated as a null-terminated length.
TEST_P(GLSLValidationTest, NegativeShaderLength)
{
    GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);

    const char *sourceArray[1] = {essl1_shaders::fs::Red()};
    GLint lengths[1]           = {-10};
    glShaderSource(shader, static_cast<GLsizei>(ArraySize(sourceArray)), sourceArray, lengths);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);
    EXPECT_NE(compileResult, 0);
}

// Verify that a length array with mixed positive and negative values compiles.
TEST_P(GLSLValidationTest, MixedShaderLengths)
{
    GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);

    const char *sourceArray[] = {
        "void main()",
        "{",
        "    gl_FragColor = vec4(0, 0, 0, 0);",
        "}",
    };
    GLint lengths[] = {
        -10,
        1,
        static_cast<GLint>(strlen(sourceArray[2])),
        -1,
    };
    ASSERT_EQ(ArraySize(sourceArray), ArraySize(lengths));

    glShaderSource(shader, static_cast<GLsizei>(ArraySize(sourceArray)), sourceArray, lengths);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);
    EXPECT_NE(compileResult, 0);
}

// Verify that zero-length shader source does not affect shader compilation.
TEST_P(GLSLValidationTest, ZeroShaderLength)
{
    GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);

    const char *sourceArray[] = {
        "abcdefg", "34534", "void main() { gl_FragColor = vec4(0, 0, 0, 0); }", "", "abcdefghijklm",
    };
    GLint lengths[] = {
        0, 0, -1, 0, 0,
    };
    ASSERT_EQ(ArraySize(sourceArray), ArraySize(lengths));

    glShaderSource(shader, static_cast<GLsizei>(ArraySize(sourceArray)), sourceArray, lengths);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);
    EXPECT_NE(compileResult, 0);
}

// Test that structs defined in uniforms are translated correctly.
TEST_P(GLSLValidationTest, StructSpecifiersUniforms)
{
    constexpr char kFS[] = R"(precision mediump float;

uniform struct S { float field; } s;

void main()
{
    gl_FragColor = vec4(1, 0, 0, 1);
    gl_FragColor.a += s.field;
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that if a non-preprocessor token is seen in a disabled if-block then it does not disallow
// extension pragmas later
TEST_P(GLSLValidationTest, NonPreprocessorTokensInIfBlocks)
{
    constexpr const char *kFS = R"(
#if __VERSION__ >= 300
    inout mediump vec4 fragData;
#else
    #extension GL_EXT_shader_texture_lod :enable
#endif

void main()
{
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that two constructors which have vec4 and mat2 parameters get disambiguated (issue in
// HLSL).
TEST_P(GLSLValidationTest_ES3, AmbiguousConstructorCall2x2)
{
    constexpr char kVS[] = R"(#version 300 es
precision highp float;
in vec4 a_vec;
in mat2 a_mat;
void main()
{
    gl_Position = vec4(a_vec) + vec4(a_mat);
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Test that two constructors which have mat2x3 and mat3x2 parameters get disambiguated.
// This was suspected to be an issue in HLSL, but HLSL seems to be able to natively choose between
// the function signatures in this case.
TEST_P(GLSLValidationTest_ES3, AmbiguousConstructorCall2x3)
{
    constexpr char kVS[] = R"(#version 300 es
precision highp float;
in mat3x2 a_matA;
in mat2x3 a_matB;
void main()
{
    gl_Position = vec4(a_matA) + vec4(a_matB);
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Test that two functions which have vec4 and mat2 parameters get disambiguated (issue in HLSL).
TEST_P(GLSLValidationTest_ES3, AmbiguousFunctionCall2x2)
{
    constexpr char kVS[] = R"(#version 300 es
precision highp float;
in vec4 a_vec;
in mat2 a_mat;
vec4 foo(vec4 a)
{
    return a;
}
vec4 foo(mat2 a)
{
    return vec4(a[0][0]);
}
void main()
{
    gl_Position = foo(a_vec) + foo(a_mat);
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Test that an user-defined function with a large number of float4 parameters doesn't fail due to
// the function name being too long.
TEST_P(GLSLValidationTest_ES3, LargeNumberOfFloat4Parameters)
{
    std::stringstream vs;
    // Note: SPIR-V doesn't allow more than 255 parameters to a function.
    const unsigned int paramCount = (IsVulkan() || IsMetal()) ? 255u : 1024u;

    vs << R"(#version 300 es
precision highp float;
in vec4 a_vec;
vec4 lotsOfVec4Parameters()";
    for (unsigned int i = 0; i < paramCount - 1; ++i)
    {
        vs << "vec4 a" << i << ", ";
    }
    vs << R"(vec4 aLast)
{
    vec4 sum = vec4(0.0, 0.0, 0.0, 0.0);)";
    for (unsigned int i = 0; i < paramCount - 1; ++i)
    {
        vs << "    sum += a" << i << ";\n";
    }
    vs << R"(    sum += aLast;
    return sum;
}
void main()
{
    gl_Position = lotsOfVec4Parameters()";
    for (unsigned int i = 0; i < paramCount - 1; ++i)
    {
        vs << "a_vec, ";
    }
    vs << R"(a_vec);
})";
    validateSuccess(GL_VERTEX_SHADER, vs.str().c_str());
}

// This test was written specifically to stress DeferGlobalInitializers AST transformation.
// Test a shader where a global constant array is initialized with an expression containing array
// indexing. This initializer is tricky to constant fold, so if it's not constant folded it needs to
// be handled in a way that doesn't generate statements in the global scope in HLSL output.
// Also includes multiple array initializers in one declaration, where only the second one has
// array indexing. This makes sure that the qualifier for the declaration is set correctly if
// transformations are applied to the declaration also in the case of ESSL output.
TEST_P(GLSLValidationTest_ES3, InitGlobalArrayWithArrayIndexing)
{
    // TODO(ynovikov): re-enable once root cause of http://anglebug.com/42260423 is fixed
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsAdreno() && IsOpenGLES());

    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 my_FragColor;
const highp float f[2] = float[2](0.1, 0.2);
const highp float[2] g = float[2](0.3, 0.4), h = float[2](0.5, f[1]);
void main()
{
    my_FragColor = vec4(h[1]);
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that index-constant sampler array indexing is supported.
TEST_P(GLSLValidationTest, IndexConstantSamplerArrayIndexing)
{
    constexpr char kFS[] = R"(
precision mediump float;
uniform sampler2D uni[2];

float zero(int x)
{
    return float(x) - float(x);
}

void main()
{
    vec4 c = vec4(0,0,0,0);
    for (int ii = 1; ii < 3; ++ii) {
        if (c.x > 255.0) {
            c.x = 255.0 + zero(ii);
            break;
        }
        // Index the sampler array with a predictable loop index (index-constant) as opposed to
        // a true constant. This is valid in OpenGL ES but isn't in many Desktop OpenGL versions,
        // without an extension.
        c += texture2D(uni[ii - 1], vec2(0.5, 0.5));
    }
    gl_FragColor = c;
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that the #pragma directive is supported and doesn't trigger a compilation failure on the
// native driver. The only pragma that gets passed to the OpenGL driver is "invariant" but we don't
// want to test its behavior, so don't use any varyings.
TEST_P(GLSLValidationTest, PragmaDirective)
{
    constexpr char kVS[] = R"(#pragma STDGL invariant(all)
void main()
{
    gl_Position = vec4(1.0, 0.0, 0.0, 1.0);
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Tests that using a constant declaration as the only statement in a for loop without curly braces
// doesn't crash.
TEST_P(GLSLValidationTest, ConstantStatementInForLoop)
{
    constexpr char kVS[] = R"(void main()
{
    for (int i = 0; i < 10; ++i)
        const int b = 0;
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Tests that using a constant declaration as a loop init expression doesn't crash. Note that this
// test doesn't work on D3D9 due to looping limitations, so it is only run on ES3.
TEST_P(GLSLValidationTest_ES3, ConstantStatementAsLoopInit)
{
    constexpr char kVS[] = R"(void main()
{
    for (const int i = 0; i < 0;) {}
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Tests that using a constant condition guarding a discard works
// Covers a failing case in the Vulkan backend: http://anglebug.com/42265506
TEST_P(GLSLValidationTest_ES3, ConstantConditionGuardingDiscard)
{
    constexpr char kFS[] = R"(#version 300 es
void main()
{
    if (true)
    {
        discard;
    }
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Tests that nesting a discard in unconditional blocks works
// Covers a failing case in the Vulkan backend: http://anglebug.com/42265506
TEST_P(GLSLValidationTest_ES3, NestedUnconditionalDiscards)
{
    constexpr char kFS[] = R"(#version 300 es
out mediump vec4 c;
void main()
{
    {
        c = vec4(0);
        {
            discard;
        }
    }
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Tests that rewriting samplers in structs works when passed as function argument.  In this test,
// the function references another struct, which is not being modified.  Regression test for AST
// validation applied to a multipass transformation, where references to declarations were attempted
// to be validated without having the entire shader.  In this case, the reference to S2 was flagged
// as invalid because S2's declaration was not visible.
TEST_P(GLSLValidationTest, SamplerInStructAsFunctionArg)
{
    const char kFS[] = R"(precision mediump float;
struct S { sampler2D samp; bool b; };
struct S2 { float f; };

uniform S us;

float f(S s)
{
    S2 s2;
    s2.f = float(s.b);
    return s2.f;
}

void main()
{
    gl_FragColor = vec4(f(us), 0, 0, 1);
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test a fuzzer-discovered bug with the VectorizeVectorScalarArithmetic transformation.
TEST_P(GLSLValidationTest, VectorScalarArithmeticWithSideEffectInLoop)
{
    // The VectorizeVectorScalarArithmetic transformation was generating invalid code in the past
    // (notice how sbcd references i outside the for loop.  The loop condition doesn't look right
    // either):
    //
    //     #version 450
    //     void main(){
    //     (gl_Position = vec4(0.0, 0.0, 0.0, 0.0));
    //     mat3 _utmp = mat3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    //     vec3 _ures = vec3(0.0, 0.0, 0.0);
    //     vec3 sbcd = vec3(_ures[_ui]);
    //     for (int _ui = 0; (_ures[((_utmp[_ui] += (((sbcd *= _ures[_ui]), (_ures[_ui] = sbcd.x)),
    //     sbcd)), _ui)], (_ui < 7)); )
    //     {
    //     }
    //     }

    constexpr char kVS[] = R"(
void main()
{
    mat3 tmp;
    vec3 res;
    for(int i; res[tmp[i]+=res[i]*=res[i],i],i<7;);
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Test that inactive output variables compile ok in combination with initOutputVariables
// (which is enabled on WebGL).
TEST_P(WebGL2GLSLValidationTest, InactiveOutput)
{
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 _cassgl_2_;
void main()
{
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that output variables declared after main work in combination with initOutputVariables
// (which is enabled on WebGL).
TEST_P(WebGLGLSLValidationTest, OutputAfterMain)
{
    constexpr char kVS[] = R"(void main(){}
varying float r;)";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Test angle can handle big initial stack size with dynamic stack allocation.
TEST_P(GLSLValidationTest, MemoryExhaustedTest)
{
    constexpr int kLength = 36;

    std::stringstream fs;
    fs << "void main() {\n";
    for (int i = 0; i < kLength; i++)
    {
        fs << "  if (true) {\n";
    }
    fs << "  int temp;\n";
    for (int i = 0; i <= kLength; i++)
    {
        fs << "}";
    }
    validateSuccess(GL_FRAGMENT_SHADER, fs.str().c_str());
}

// Test that separating declarators works with structs that have been separately defined.
TEST_P(GLSLValidationTest_ES31, SeparateDeclaratorsOfStructType)
{
    constexpr char kVS[] = R"(#version 310 es
precision highp float;

struct S
{
    mat4 a;
    mat4 b;
};

S s1 = S(mat4(1), mat4(2)), s2[2][3], s3[2] = S[2](S(mat4(0), mat4(3)), S(mat4(4), mat4(5)));

void main() {
    S s4[2][3] = s2, s5 = s3[0], s6[2] = S[2](s1, s5), s7 = s5;

    gl_Position = vec4(s3[1].a[0].x, s2[0][2].b[1].y, s4[1][0].a[2].z, s6[0].b[3].w);
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Test that separating declarators works with structs that are simultaneously defined.
TEST_P(GLSLValidationTest_ES31, SeparateDeclaratorsOfStructTypeBeingSpecified)
{
    constexpr char kVS[] = R"(#version 310 es
precision highp float;

struct S
{
    mat4 a;
    mat4 b;
} s1 = S(mat4(1), mat4(2)), s2[2][3], s3[2] = S[2](S(mat4(0), mat4(3)), S(mat4(4), mat4(5)));

void main() {
    struct T
    {
        mat4 a;
        mat4 b;
    } s4[2][3], s5 = T(s3[0].a, s3[0].b), s6[2] = T[2](T(s1.a, s1.b), s5), s7 = s5;

    float f1 = s3[1].a[0].x, f2 = s2[0][2].b[1].y;

    gl_Position = vec4(f1, f2, s4[1][0].a[2].z, s6[0].b[3].w);
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Test that separating declarators works with structs that are simultaneously defined and that are
// nameless.
TEST_P(GLSLValidationTest_ES31, SeparateDeclaratorsOfNamelessStructType)
{
    constexpr char kVS[] = R"(#version 310 es
precision highp float;

struct
{
    mat4 a;
    mat4 b;
} s1, s2[2][3], s3[2];

void main() {
    struct
    {
        mat4 a;
        mat4 b;
    } s4[2][3], s5, s6[2], s7 = s5;

    float f1 = s1.a[0].x + s3[1].a[0].x, f2 = s2[0][2].b[1].y + s7.b[1].z;

    gl_Position = vec4(f1, f2, s4[1][0].a[2].z, s6[0].b[3].w);
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Regression test for transformation bug which separates struct declarations from uniform
// declarations.  The bug was that the uniform variable usage in the initializer of a new
// declaration (y below) was not being processed.
TEST_P(GLSLValidationTest, UniformStructBug)
{
    constexpr char kVS[] = R"(precision highp float;

uniform struct Global
{
    float x;
} u_global;

void main() {
  float y = u_global.x;

  gl_Position = vec4(y);
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Regression test for transformation bug which separates struct declarations from uniform
// declarations.  The bug was that the arrayness of the declaration was not being applied to the
// replaced uniform variable.
TEST_P(GLSLValidationTest_ES31, UniformStructBug2)
{
    constexpr char kVS[] = R"(#version 310 es
precision highp float;

uniform struct Global
{
    float x;
} u_global[2][3];

void main() {
  float y = u_global[0][0].x;

  gl_Position = vec4(y);
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Regression test based on fuzzer issue resulting in an AST validation failure.  Struct definition
// was not found in the tree.  Tests that struct declaration in function return value is visible to
// instantiations later on.
TEST_P(GLSLValidationTest, MissingStructDeclarationBug)
{
    constexpr char kVS[] = R"(
struct S
{
    vec4 i;
} p();
void main()
{
    S s;
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Regression test based on fuzzer issue resulting in an AST validation failure.  Struct definition
// was not found in the tree.  Tests that struct declaration in function return value is visible to
// other struct declarations.
TEST_P(GLSLValidationTest, MissingStructDeclarationBug2)
{
    constexpr char kVS[] = R"(
struct T
{
    vec4 I;
} p();
struct
{
    T c;
};
void main()
{
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Regression test for bug in HLSL code generation where the for loop init expression was expected
// to always have an initializer.
TEST_P(GLSLValidationTest, HandleExcessiveLoopBug)
{
    constexpr char kVS[] = R"(void main(){for(int i;i>6;);})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Test that providing more components to a matrix constructor than necessary works.  Based on a
// clusterfuzz test that caught an OOB array write in glslang.
TEST_P(GLSLValidationTest, MatrixConstructor)
{
    constexpr char kVS[] = R"(attribute vec4 aPosition;
varying vec4 vColor;
void main()
{
    gl_Position = aPosition;
    vec4 color = vec4(aPosition.xy, 0, 1);
    mat4 m4 = mat4(color, color.yzwx, color.zwx, color.zwxy, color.wxyz);
    vColor = m4[0];
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Test constructors without precision
TEST_P(GLSLValidationTest, ConstructFromBoolVector)
{
    constexpr char kFS[] = R"(precision mediump float;
uniform float u;
void main()
{
    mat4 m = mat4(u);
    mat2(0, bvec3(m));
    gl_FragColor = vec4(m);
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test constructing vector from matrix
TEST_P(GLSLValidationTest, VectorConstructorFromMatrix)
{
    constexpr char kFS[] = R"(precision mediump float;
uniform mat2 umat2;
void main()
{
    gl_FragColor = vec4(umat2);
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that initializing global variables with non-constant values work
TEST_P(GLSLValidationTest_ES3, InitGlobalNonConstant)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_non_constant_global_initializers"));

    constexpr char kVS[] = R"(#version 300 es
#extension GL_EXT_shader_non_constant_global_initializers : require
uniform vec4 u;
out vec4 color;

vec4 global1 = u;
vec4 global2 = u + vec4(1);
vec4 global3 = global1 * global2;
void main()
{
    color = global3;
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Regression test for a crash in SPIR-V output when faced with an array of struct constant.
TEST_P(GLSLValidationTest_ES3, ArrayOfStructConstantBug)
{
    constexpr char kFS[] = R"(#version 300 es
struct S {
    int foo;
};
void main() {
    S a[3];
    a = S[3](S(0), S(1), S(2));
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Regression test for a bug in SPIR-V output where float+matrix was mishandled.
TEST_P(GLSLValidationTest_ES3, FloatPlusMatrix)
{
    constexpr char kFS[] = R"(#version 300 es

precision mediump float;

layout(location=0) out vec4 color;

uniform float f;

void main()
{
    mat3x2 m = f + mat3x2(0);
    color = vec4(m[0][0]);
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Regression test for a bug in SPIR-V output where a transformation creates float(constant) without
// folding it into a TIntermConstantUnion.  This transformation is clamping non-constant indices in
// WebGL.  The |false ? i : 5| as index caused the transformation to consider this a non-constant
// index.
TEST_P(WebGL2GLSLValidationTest, IndexClampConstantIndexBug)
{
    constexpr char kFS[] = R"(#version 300 es
precision highp float;

layout(location=0) out float f;

uniform int i;

void main()
{
    float data[10];
    f = data[false ? i : 5];
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that framebuffer fetch transforms gl_LastFragData in the presence of gl_FragCoord without
// failing validation (adapted from a Chromium test, see anglebug.com/42265427)
TEST_P(GLSLValidationTest, FramebufferFetchWithLastFragData)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));

    constexpr char kFS[] = R"(#version 100

#extension GL_EXT_shader_framebuffer_fetch : require
varying mediump vec4 color;
void main() {
    gl_FragColor = length(gl_FragCoord.xy) * gl_LastFragData[0];
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that loop body ending in a branch doesn't fail compilation
TEST_P(GLSLValidationTest, LoopBodyEndingInBranch1)
{
    constexpr char kFS[] = R"(void main(){for(int a,i;;gl_FragCoord)continue;})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that loop body ending in a branch doesn't fail compilation
TEST_P(GLSLValidationTest, LoopBodyEndingInBranch2)
{
    constexpr char kFS[] =
        R"(void main(){for(int a,i;bool(gl_FragCoord.x);gl_FragCoord){continue;}})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that loop body ending in a branch doesn't fail compilation
TEST_P(GLSLValidationTest, LoopBodyEndingInBranch3)
{
    constexpr char kFS[] = R"(void main(){for(int a,i;;gl_FragCoord){{continue;}}})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that loop body ending in a branch doesn't fail compilation
TEST_P(GLSLValidationTest, LoopBodyEndingInBranch4)
{
    constexpr char kFS[] = R"(void main(){for(int a,i;;gl_FragCoord){{continue;}{}{}{{}{}}}})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that loop body ending in a branch doesn't fail compilation
TEST_P(GLSLValidationTest, LoopBodyEndingInBranch5)
{
    constexpr char kFS[] = R"(void main(){while(bool(gl_FragCoord.x)){{continue;{}}{}}})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that loop body ending in a branch doesn't fail compilation
TEST_P(GLSLValidationTest, LoopBodyEndingInBranch6)
{
    constexpr char kFS[] = R"(void main(){do{{continue;{}}{}}while(bool(gl_FragCoord.x));})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Fuzzer test involving struct samplers and comma operator
TEST_P(GLSLValidationTest, StructSamplerVsComma)
{
    constexpr char kVS[] = R"(uniform struct S1
{
    samplerCube ar;
    vec2 c;
} a;

struct S2
{
    vec3 c;
} b[2];

void main (void)
{
    ++b[0].c,a;
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Regression test for a bug where the sampler-in-struct rewrite transformation did not take a
// specific pattern of side_effect,index_the_struct_to_write into account.
TEST_P(GLSLValidationTest_ES3, StructWithSamplerRHSOfCommaWithSideEffect)
{
    constexpr char kVS[] = R"(uniform struct S {
    sampler2D s;
    mat2 m;
} u[2];
void main()
{
    ++gl_Position, u[0];
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Regression test for a bug where the sampler-in-struct rewrite transformation did not take a
// specific pattern of side_effect,struct_with_only_samplers into account.
TEST_P(GLSLValidationTest_ES3, StructWithOnlySamplersRHSOfCommaWithSideEffect)
{
    constexpr char kVS[] = R"(uniform struct S {
    sampler2D s;
} u;
void main()
{
    ++gl_Position, u;
})";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Test that gl_FragDepth can be marked invariant.
TEST_P(GLSLValidationTest_ES3, FragDepthInvariant)
{
    constexpr char kFS[] = R"(#version 300 es
#extension GL_EXT_conservative_depth: enable
precision mediump float;
invariant gl_FragDepth;
void main() {
    gl_FragDepth = 0.5;
}
)";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that gl_Position and gl_PointSize can be marked invariant and redeclared in separate
// statements. Note that EXT_seperate_shader_objects expects the redeclaration to come first.
TEST_P(GLSLValidationTest_ES31, PositionRedeclaredAndInvariant)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_separate_shader_objects"));

    constexpr char kVS[] = R"(#version 310 es
#extension GL_EXT_separate_shader_objects: require
precision mediump float;
out vec4 gl_Position;
out float gl_PointSize;
invariant gl_Position;
invariant gl_PointSize;
void main() {
}
)";
    validateSuccess(GL_VERTEX_SHADER, kVS);
}

// Test an invalid shader where a for loop index is used as an out parameter.
// See limitations in ESSL 1.00 Appendix A.
TEST_P(WebGLGLSLValidationTest, IndexAsFunctionOutParameter)
{
    const char kFS[] = R"(precision mediump float;
void fun(out int a)
{
   a = 2;
}
void main()
{
    for (int i = 0; i < 2; ++i)
    {
        fun(i);
    }
    gl_FragColor = vec4(0.0);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'i' : Loop index cannot be statically assigned to within the body of the loop");
}

// Test an invalid shader where a for loop index is used as an inout parameter.
// See limitations in ESSL 1.00 Appendix A.
TEST_P(WebGLGLSLValidationTest, IndexAsFunctionInOutParameter)
{
    const char kFS[] = R"(precision mediump float;
void fun(int b, inout int a)
{
   a += b;
}
void main()
{
    for (int i = 0; i < 2; ++i)
    {
        fun(2, i);
    }
    gl_FragColor = vec4(0.0);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'i' : Loop index cannot be statically assigned to within the body of the loop");
}

// Test a valid shader where a for loop index is used as an in parameter in a function that also has
// an out parameter.
// See limitations in ESSL 1.00 Appendix A.
TEST_P(WebGLGLSLValidationTest, IndexAsFunctionInParameter)
{
    const char kFS[] = R"(precision mediump float;
void fun(int b, inout int a)
{
   a += b;
}
void main()
{
    for (int i = 0; i < 2; ++i)
    {
        int a = 1;
        fun(i, a);
    }
    gl_FragColor = vec4(0.0);
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test an invalid shader where a for loop index is used as a target of assignment.
// See limitations in ESSL 1.00 Appendix A.
TEST_P(WebGLGLSLValidationTest, IndexAsTargetOfAssignment)
{
    const char kFS[] = R"(precision mediump float;
void main()
{
    for (int i = 0; i < 2; ++i)
    {
        i = 2;
    }
    gl_FragColor = vec4(0.0);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'i' : Loop index cannot be statically assigned to within the body of the loop");
}

// Test an invalid shader where a for loop index is incremented inside the loop.
// See limitations in ESSL 1.00 Appendix A.
TEST_P(WebGLGLSLValidationTest, IndexIncrementedInLoopBody)
{
    const char kFS[] = R"(precision mediump float;
void main()
{
    for (int i = 0; i < 2; ++i)
    {
        ++i;
    }
    gl_FragColor = vec4(0.0);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'i' : Loop index cannot be statically assigned to within the body of the loop");
}

// Shader that writes to SecondaryFragColor and SecondaryFragData does not compile.
TEST_P(GLSLValidationTest, BlendFuncExtendedSecondaryColorAndData)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    const char kFS[] = R"(#extension GL_EXT_blend_func_extended : require
precision mediump float;
void main() {
    gl_SecondaryFragColorEXT = vec4(1.0);
    gl_SecondaryFragDataEXT[gl_MaxDualSourceDrawBuffersEXT - 1] = vec4(0.1);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "cannot use both output variable sets (gl_FragData, gl_SecondaryFragDataEXT) and "
                  "(gl_FragColor, gl_SecondaryFragColorEXT)");
}

// Shader that writes to FragColor and SecondaryFragData does not compile.
TEST_P(GLSLValidationTest, BlendFuncExtendedColorAndSecondaryData)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    const char kFS[] = R"(#extension GL_EXT_blend_func_extended : require
precision mediump float;
void main() {
    gl_FragColor = vec4(1.0);
    gl_SecondaryFragDataEXT[gl_MaxDualSourceDrawBuffersEXT - 1] = vec4(0.1);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "cannot use both output variable sets (gl_FragData, gl_SecondaryFragDataEXT) and "
                  "(gl_FragColor, gl_SecondaryFragColorEXT)");
}

// Shader that writes to FragData and SecondaryFragColor.
TEST_P(GLSLValidationTest, BlendFuncExtendedDataAndSecondaryColor)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_draw_buffers"));

    const char kFS[] = R"(#extension GL_EXT_draw_buffers : require
#extension GL_EXT_blend_func_extended : require
precision mediump float;
void main() {
    gl_SecondaryFragColorEXT = vec4(1.0);
    gl_FragData[gl_MaxDrawBuffers - 1] = vec4(0.1);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "cannot use both output variable sets (gl_FragData, gl_SecondaryFragDataEXT) and "
                  "(gl_FragColor, gl_SecondaryFragColorEXT)");
}

// Dynamic indexing of SecondaryFragData is not allowed in WebGL 2.0.
TEST_P(WebGL2GLSLValidationTest, BlendFuncExtendedSecondaryDataIndexing)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    const char kFS[] = R"(#extension GL_EXT_blend_func_extended : require
precision mediump float;
void main() {
    for (int i = 0; i < 2; ++i) {
        gl_SecondaryFragDataEXT[true ? 0 : i] = vec4(0.0);
    }
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "array index for gl_SecondaryFragDataEXT must be constant zero");
}

// Shader that specifies index layout qualifier but not location fails to compile.
TEST_P(GLSLValidationTest_ES3, BlendFuncExtendedNoLocationQualifier)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
layout(index = 0) out vec4 fragColor;
void main() {
    fragColor = vec4(1.0);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'index' : If index layout qualifier is specified for a fragment output, "
                  "location must also be specified");
}

// Shader that specifies index layout qualifier multiple times fails to compile.
TEST_P(GLSLValidationTest_ES3, BlendFuncExtendedMultipleIndexQualifiers)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
layout(index = 0, location = 0, index = 1) out vec4 fragColor;
void main() {
    fragColor = vec4(1.0);
})";
    validateError(GL_FRAGMENT_SHADER, kFS, "'index' : Cannot have multiple index specifiers");
}

// Shader that specifies an output with out-of-bounds location
// for index 0 when another output uses index 1 is invalid.
TEST_P(GLSLValidationTest_ES3, BlendFuncExtendedOutOfBoundsLocationQualifier)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    GLint maxDualSourceDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT, &maxDualSourceDrawBuffers);
    ANGLE_SKIP_TEST_IF(maxDualSourceDrawBuffers > 1);

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
layout(location = 1, index = 0) out mediump vec4 fragColor;
layout(location = 0, index = 1) out mediump vec4 secondaryFragColor;
void main() {
    fragColor = vec4(1);
    secondaryFragColor = vec4(1);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'fragColor' : output location must be < MAX_DUAL_SOURCE_DRAW_BUFFERS");
}

// Shader that specifies an output with out-of-bounds location for index 1 is invalid.
TEST_P(GLSLValidationTest_ES3, BlendFuncExtendedOutOfBoundsLocationQualifierIndex1)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    GLint maxDualSourceDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT, &maxDualSourceDrawBuffers);
    ANGLE_SKIP_TEST_IF(maxDualSourceDrawBuffers > 1);

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
layout(location = 1, index = 1) out mediump vec4 secondaryFragColor;
void main() {
    secondaryFragColor = vec4(1);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'secondaryFragColor' : output location must be < MAX_DUAL_SOURCE_DRAW_BUFFERS");
}

// Shader that specifies two outputs with the same location
// but different indices and different base types is invalid.
TEST_P(GLSLValidationTest_ES3, BlendFuncExtendedLocationOverlap)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
layout(location = 0, index = 0) out mediump vec4 fragColor;
layout(location = 0, index = 1) out mediump ivec4 secondaryFragColor;
void main() {
    fragColor = vec4(1);
    secondaryFragColor = ivec4(1);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'secondaryFragColor' : conflicting output types with previously defined output "
                  "'fragColor' for location 0");
}

// Global index layout qualifier fails.
TEST_P(GLSLValidationTest_ES3, BlendFuncExtendedGlobalIndexQualifier)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
layout(index = 0);
out vec4 fragColor;
void main() {
    fragColor = vec4(1.0);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'index' : invalid layout qualifier: only valid when used with a fragment shader "
                  "output in ESSL version >= 3.00 and EXT_blend_func_extended is enabled");
}

// Index layout qualifier on a non-output variable fails.
TEST_P(GLSLValidationTest_ES3, BlendFuncExtendedIndexQualifierOnUniform)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
layout(index = 0) uniform vec4 u;
out vec4 fragColor;
void main() {
    fragColor = u;
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'index' : invalid layout qualifier: only valid when used with a fragment shader "
                  "output in ESSL version >= 3.00 and EXT_blend_func_extended is enabled");
}

// Index layout qualifier on a struct fails.
TEST_P(GLSLValidationTest_ES3, BlendFuncExtendedIndexQualifierOnStruct)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
layout(index = 0) struct S {
    vec4 field;
};
out vec4 fragColor;
void main() {
    fragColor = vec4(1.0);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'index' : invalid layout qualifier: only valid when used with a fragment shader "
                  "output in ESSL version >= 3.00 and EXT_blend_func_extended is enabled");
}

// Index layout qualifier on a struct member fails.
TEST_P(GLSLValidationTest_ES3, BlendFuncExtendedIndexQualifierOnField)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
struct S {
    layout(index = 0) vec4 field;
};
out mediump vec4 fragColor;
void main() {
    fragColor = vec4(1.0);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'index' : invalid layout qualifier: only valid when used with a fragment shader "
                  "output in ESSL version >= 3.00 and EXT_blend_func_extended is enabled");
}

// Shader that specifies yuv layout qualifier for not output fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetYuvQualifierOnInput)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
layout(yuv) in vec4 fragColor;
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'yuv' : invalid layout qualifier: only valid on program outputs");
}

// Shader that specifies yuv layout qualifier for not output fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetYuvQualifierOnUniform)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
layout(yuv) uniform;
layout(yuv) uniform Transform {
     mat4 M1;
};
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'yuv' : invalid layout qualifier: only valid on program outputs");
}

// Shader that specifies yuv layout qualifier with location fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetYuvQualifierAndLocation)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
layout(location = 0, yuv) out vec4 fragColor;
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS, "'yuv' : invalid layout qualifier combination");
}

// Shader that specifies yuv layout qualifier with multiple color outputs fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetYuvAndColorOutput)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
layout(yuv) out vec4 fragColor;
out vec4 fragColor1;
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'fragColor' : not allowed to specify yuv qualifier when using depth or multiple "
                  "color fragment outputs");
}

// Shader that specifies yuv layout qualifier with multiple color outputs fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetYuvAndColorOutputWithLocation)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
layout(yuv) out vec4 fragColor;
layout(location = 1) out vec4 fragColor1;
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'fragColor' : not allowed to specify yuv qualifier when using depth or multiple "
                  "color fragment outputs");
}

// Shader that specifies yuv layout qualifier with depth output fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetWithFragDepth)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
layout(yuv) out vec4 fragColor;
void main() {
    gl_FragDepth = 1.0f;
})";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'fragColor' : not allowed to specify yuv qualifier when using depth or multiple "
                  "color fragment outputs");
}

// Shader that specifies yuv layout qualifier with multiple outputs fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetMultipleYuvOutputs)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
layout(yuv) out vec4 fragColor;
layout(yuv) out vec4 fragColor1;
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'fragColor' : not allowed to specify yuv qualifier when using depth or multiple "
                  "color fragment outputs");
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'fragColor1' : not allowed to specify yuv qualifier when using depth or "
                  "multiple color fragment outputs");
}

// Shader that specifies yuvCscStandardEXT type constructor fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetEmptyCscStandardConstructor)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
yuvCscStandardEXT conv = yuvCscStandardEXT();
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS, "'yuvCscStandardEXT' : cannot construct this type");
}

// Shader that specifies yuvCscStandardEXT type constructor fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetCscStandardConstructor)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
yuvCscStandardEXT conv = yuvCscStandardEXT(itu_601);
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS, "'yuvCscStandardEXT' : cannot construct this type");
}

// Shader that specifies yuvCscStandardEXT type conversion fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetImplicitTypeConversionToCscStandardFromBool)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
yuvCscStandardEXT conv = false;
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "cannot convert from 'const bool' to 'yuvCscStandardEXT'");
}

// Shader that specifies yuvCscStandardEXT type conversion fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetImplicitTypeConversionToCscStandardFromInt)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
yuvCscStandardEXT conv = 0;
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "cannot convert from 'const int' to 'yuvCscStandardEXT'");
}

// Shader that specifies yuvCscStandardEXT type conversion fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetImplicitTypeConversionToCscStandardFromFloat)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
yuvCscStandardEXT conv = 2.0f;
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "cannot convert from 'const float' to 'yuvCscStandardEXT'");
}

// Shader that does arithmetics on yuvCscStandardEXT fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetCscStandardOr)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
yuvCscStandardEXT conv = itu_601 | itu_709;
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "wrong operand types - no operation '|' exists that takes a left-hand operand of "
                  "type 'const yuvCscStandardEXT' and a right operand of type 'const "
                  "yuvCscStandardEXT' (or there is no acceptable conversion)");
}

// Shader that does arithmetics on yuvCscStandardEXT fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetCscStandardAnd)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
yuvCscStandardEXT conv = itu_601 & 3.0f;
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "wrong operand types - no operation '&' exists that takes a left-hand operand of "
                  "type 'const yuvCscStandardEXT' and a right operand of type 'const float' (or "
                  "there is no acceptable conversion)");
}

// Shader that specifies yuvCscStandardEXT type qualifiers fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetCscStandardInput)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
in yuvCscStandardEXT conv = itu_601;
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS, "'in' : cannot be used with a yuvCscStandardEXT");
}

// Shader that specifies yuvCscStandardEXT type qualifiers fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetCscStandardOutput)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
out yuvCscStandardEXT conv = itu_601;
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS, "'out' : cannot be used with a yuvCscStandardEXT");
}

// Shader that specifies yuvCscStandardEXT type qualifiers fails to compile.
TEST_P(GLSLValidationTest_ES3, YUVTargetCscStandardUniform)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_YUV_target : require
precision mediump float;
uniform yuvCscStandardEXT conv = itu_601;
void main() {
})";

    validateError(GL_FRAGMENT_SHADER, kFS, "'uniform' : cannot be used with a yuvCscStandardEXT");
}

// Overloading rgb_2_yuv is ok if the extension is not supported.
TEST_P(GLSLValidationTest_ES3, OverloadRgb2Yuv)
{
    ANGLE_SKIP_TEST_IF(IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] = R"(#version 300 es
precision mediump float;
float rgb_2_yuv(float x) { return x + 1.0; }

in float i;
out float o;

void main()
{
    o = rgb_2_yuv(i);
})";

    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Overloading yuv_2_rgb is ok if the extension is not supported.
TEST_P(GLSLValidationTest_ES3, OverloadYuv2Rgb)
{
    ANGLE_SKIP_TEST_IF(IsGLExtensionEnabled("GL_EXT_YUV_target"));

    const char kFS[] = R"(#version 300 es
precision mediump float;
float yuv_2_rgb(float x) { return x + 1.0; }

in float i;
out float o;

void main()
{
    o = yuv_2_rgb(i);
})";

    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Use gl_LastFragData without redeclaration of gl_LastFragData with noncoherent qualifier
TEST_P(GLSLValidationTest, FramebufferFetchNoLastFragDataRedeclaration)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));

    const char kFS[] =
        R"(#extension GL_EXT_shader_framebuffer_fetch_non_coherent : require
uniform highp vec4 u_color;

void main (void)
{
    gl_FragColor = u_color + gl_LastFragData[0];
})";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'noncoherent' qualifier must be used when "
                  "GL_EXT_shader_framebuffer_fetch_non_coherent extension is used");
}

// Redeclare gl_LastFragData without noncoherent qualifier
TEST_P(GLSLValidationTest, FramebufferFetchLastFragDataWithoutNoncoherentQualifier)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));

    const char kFS[] =
        R"(#extension GL_EXT_shader_framebuffer_fetch_non_coherent : require
uniform highp vec4 u_color;
highp vec4 gl_LastFragData[gl_MaxDrawBuffers];

void main (void)
{
    gl_FragColor = u_color + gl_LastFragData[0];
})";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'noncoherent' qualifier must be used when "
                  "GL_EXT_shader_framebuffer_fetch_non_coherent extension is used");
}

// Declare inout without noncoherent qualifier
TEST_P(GLSLValidationTest_ES3, FramebufferFetchInoutWithoutNoncoherentQualifier)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));

    const char kFS[] =
        R"(#version 300 es
#extension GL_EXT_shader_framebuffer_fetch_non_coherent : require
layout(location = 0) inout highp vec4 o_color;
uniform highp vec4 u_color;

void main (void)
{
    o_color = clamp(o_color + u_color, vec4(0.0f), vec4(1.0f));
})";

    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'noncoherent' qualifier must be used when "
                  "GL_EXT_shader_framebuffer_fetch_non_coherent extension is used");
}

// Validate that clip/cull distance extensions are not available in ESSL 100
TEST_P(GLSLValidationTest, ClipCullDistance)
{
    if (IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance"))
    {
        const char kVS[] = R"(#extension GL_ANGLE_clip_cull_distance : require
attribute vec4 aPosition;
void main()
{
    gl_Position = aPosition;
})";
        validateError(GL_VERTEX_SHADER, kVS,
                      "'GL_ANGLE_clip_cull_distance' : extension is not supported");
    }

    if (IsGLExtensionEnabled("GL_EXT_clip_cull_distance"))
    {
        const char kVS[] = R"(#extension GL_EXT_clip_cull_distance : require
attribute vec4 aPosition;
void main()
{
    gl_Position = aPosition;
})";
        validateError(GL_VERTEX_SHADER, kVS,
                      "'GL_EXT_clip_cull_distance' : extension is not supported");
    }
}

class GLSLValidationClipDistanceTest_ES3 : public GLSLValidationTest_ES3
{
  protected:
    void validateErrorWithExt(GLenum shaderType,
                              const char *extension,
                              const char *shaderSource,
                              const char *expectedError)
    {
        std::stringstream src;
        src << R"(#version 300 es
#extension )"
            << extension << ": require\n"
            << shaderSource;
        validateError(shaderType, src.str().c_str(), expectedError);
    }
};

class GLSLValidationClipDistanceTest_ES31 : public GLSLValidationTest_ES31
{
  protected:
    void validateErrorWithExt(GLenum shaderType,
                              const char *extension,
                              const char *shaderSource,
                              const char *expectedError)
    {
        std::stringstream src;
        src << R"(#version 310 es
#extension )"
            << extension << ": require\n"
            << shaderSource;
        validateError(shaderType, src.str().c_str(), expectedError);
    }
};

// Extension support is required to compile properly.  Expect failure when it is not present.
TEST_P(GLSLValidationClipDistanceTest_ES3, CompileFailsWithoutExtension)
{
    ANGLE_SKIP_TEST_IF(IsGLExtensionEnabled("GL_APPLE_clip_distance"));

    {
        constexpr char kVS[] = R"(#extension GL_APPLE_clip_distance : require
uniform vec4 uPlane;

attribute vec4 aPosition;

void main()
{
    gl_Position = aPosition;
    gl_ClipDistance[1] = dot(aPosition, uPlane);
})";
        validateError(GL_VERTEX_SHADER, kVS,
                      "'GL_APPLE_clip_distance' : extension is not supported");
    }

    {
        constexpr char kVS[] = R"(#version 300 es
#extension GL_APPLE_clip_distance : require
uniform vec4 uPlane;

in vec4 aPosition;

void main()
{
    gl_Position = aPosition;
    gl_ClipDistance[1] = dot(aPosition, uPlane);
})";
        validateError(GL_VERTEX_SHADER, kVS,
                      "'GL_APPLE_clip_distance' : extension is not supported");
    }
}

// Extension directive is required to compile properly.  Expect failure when it is not present.
TEST_P(GLSLValidationClipDistanceTest_ES3, CompileFailsWithExtensionWithoutPragma)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_APPLE_clip_distance"));

    {
        constexpr char kVS[] = R"(uniform vec4 uPlane;

attribute vec4 aPosition;

void main()
{
    gl_Position = aPosition;
    gl_ClipDistance[1] = dot(aPosition, uPlane);
})";
        validateError(GL_VERTEX_SHADER, kVS, "'GL_APPLE_clip_distance' : extension is disabled");
    }

    {
        constexpr char kVS[] = R"(#version 300 es
uniform vec4 uPlane;

in vec4 aPosition;

void main()
{
    gl_Position = aPosition;
    gl_ClipDistance[1] = dot(aPosition, uPlane);
})";
        validateError(GL_VERTEX_SHADER, kVS, "'GL_APPLE_clip_distance' : extension is disabled");
    }
}

// Shader using gl_ClipDistance and gl_CullDistance
// But, the sum of the sizes is greater than gl_MaxCombinedClipAndCullDistances
TEST_P(GLSLValidationClipDistanceTest_ES3, TooManyCombined)
{
    const bool hasExt   = IsGLExtensionEnabled("GL_EXT_clip_cull_distance");
    const bool hasAngle = IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance");
    ANGLE_SKIP_TEST_IF(!hasExt && !hasAngle);

    GLint maxCombinedClipAndCullDistances = 0;
    glGetIntegerv(GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES_EXT, &maxCombinedClipAndCullDistances);
    ANGLE_SKIP_TEST_IF(maxCombinedClipAndCullDistances > 11);

    const char kVS[] =
        R"(uniform vec4 uPlane;

in vec4 aPosition;

void main()
{
    gl_Position = aPosition;
    gl_ClipDistance[5] = dot(aPosition, uPlane);
    gl_CullDistance[4] = dot(aPosition, uPlane);
})";
    constexpr char kExpect[] =
        "The sum of 'gl_ClipDistance' and 'gl_CullDistance' size is greater than "
        "gl_MaxCombinedClipAndCullDistance";

    if (hasAngle)
    {
        GLint maxCullDistances = 0;
        glGetIntegerv(GL_MAX_CULL_DISTANCES_EXT, &maxCullDistances);
        if (maxCullDistances > 0)
        {
            validateErrorWithExt(GL_VERTEX_SHADER, "GL_ANGLE_clip_cull_distance", kVS, kExpect);
        }
    }

    if (hasExt)
    {
        validateErrorWithExt(GL_VERTEX_SHADER, "GL_EXT_clip_cull_distance", kVS, kExpect);
    }
}

// Shader redeclares gl_ClipDistance and gl_CullDistance
// But, the sum of the sizes is greater than gl_MaxCombinedClipAndCullDistances
TEST_P(GLSLValidationClipDistanceTest_ES3, TooManyCombined2)
{
    const bool hasExt   = IsGLExtensionEnabled("GL_EXT_clip_cull_distance");
    const bool hasAngle = IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance");
    ANGLE_SKIP_TEST_IF(!hasExt && !hasAngle);

    GLint maxCombinedClipAndCullDistances = 0;
    glGetIntegerv(GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES_EXT, &maxCombinedClipAndCullDistances);
    ANGLE_SKIP_TEST_IF(maxCombinedClipAndCullDistances > 9);

    const char kVS[] =
        R"(uniform vec4 uPlane;

in vec4 aPosition;

out highp float gl_ClipDistance[5];
out highp float gl_CullDistance[4];

void main()
{
    gl_Position = aPosition;
    gl_ClipDistance[gl_MaxClipDistances - 6 + 1] = dot(aPosition, uPlane);
    gl_ClipDistance[gl_MaxClipDistances - int(aPosition.x)] = dot(aPosition, uPlane);
    gl_CullDistance[gl_MaxCullDistances - 6 + 1] = dot(aPosition, uPlane);
    gl_CullDistance[gl_MaxCullDistances - int(aPosition.x)] = dot(aPosition, uPlane);
})";
    constexpr char kExpect[] =
        "The sum of 'gl_ClipDistance' and 'gl_CullDistance' size is greater than "
        "gl_MaxCombinedClipAndCullDistance";

    if (hasAngle)
    {
        GLint maxCullDistances = 0;
        glGetIntegerv(GL_MAX_CULL_DISTANCES_EXT, &maxCullDistances);
        if (maxCullDistances > 0)
        {
            validateErrorWithExt(GL_VERTEX_SHADER, "GL_ANGLE_clip_cull_distance", kVS, kExpect);
        }
    }

    if (hasExt)
    {
        validateErrorWithExt(GL_VERTEX_SHADER, "GL_EXT_clip_cull_distance", kVS, kExpect);
    }
}

// Shader redeclares gl_ClipDistance
// But, the array size is greater than gl_MaxClipDistances
TEST_P(GLSLValidationClipDistanceTest_ES3, TooManyClip)
{
    const char kVS[] =
        R"(uniform vec4 uPlane;

in vec4 aPosition;

out highp float gl_ClipDistance[gl_MaxClipDistances + 1];

void main()
{
    gl_Position = aPosition;
    gl_ClipDistance[gl_MaxClipDistances - 6 + 1] = dot(aPosition, uPlane);
    gl_ClipDistance[gl_MaxClipDistances - int(aPosition.x)] = dot(aPosition, uPlane);
})";
    constexpr char kExpect[] = "redeclaration of gl_ClipDistance with size > gl_MaxClipDistances";

    if (IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance"))
    {
        validateErrorWithExt(GL_VERTEX_SHADER, "GL_ANGLE_clip_cull_distance", kVS, kExpect);
    }

    if (IsGLExtensionEnabled("GL_EXT_clip_cull_distance"))
    {
        validateErrorWithExt(GL_VERTEX_SHADER, "GL_EXT_clip_cull_distance", kVS, kExpect);
    }
}

// Access gl_CullDistance with integral constant index
// But, the index is gl_MaxCullDistances, greater than gl_CullDistance array size.
TEST_P(GLSLValidationClipDistanceTest_ES3, OutOfBoundsCullIndex)
{
    const char kVS[] =
        R"(uniform vec4 uPlane;

in vec4 aPosition;

void main()
{
    gl_Position = aPosition;
    gl_CullDistance[gl_MaxCullDistances] = dot(aPosition, uPlane);
})";
    constexpr char kExpect[] = "array index out of range";

    if (IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance"))
    {
        GLint maxCullDistances = 0;
        glGetIntegerv(GL_MAX_CULL_DISTANCES_EXT, &maxCullDistances);
        if (maxCullDistances > 0)
        {
            validateErrorWithExt(GL_VERTEX_SHADER, "GL_ANGLE_clip_cull_distance", kVS, kExpect);
        }
    }

    if (IsGLExtensionEnabled("GL_EXT_clip_cull_distance"))
    {
        validateErrorWithExt(GL_VERTEX_SHADER, "GL_EXT_clip_cull_distance", kVS, kExpect);
    }
}

// Shader using gl_ClipDistance and gl_CullDistance
// But, the sum of the sizes is greater than gl_MaxCombinedClipAndCullDistances
TEST_P(GLSLValidationClipDistanceTest_ES3, TooManyCombinedFS)
{
    const bool hasExt   = IsGLExtensionEnabled("GL_EXT_clip_cull_distance");
    const bool hasAngle = IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance");
    ANGLE_SKIP_TEST_IF(!hasExt && !hasAngle);

    GLint maxCombinedClipAndCullDistances = 0;
    glGetIntegerv(GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES_EXT, &maxCombinedClipAndCullDistances);
    ANGLE_SKIP_TEST_IF(maxCombinedClipAndCullDistances > 11);

    const char kFS[] = R"(out highp vec4 fragColor;

void main()
{
    fragColor = vec4(gl_ClipDistance[4], gl_CullDistance[5], 0, 1);
})";
    constexpr char kExpect[] =
        "The sum of 'gl_ClipDistance' and 'gl_CullDistance' size is greater than "
        "gl_MaxCombinedClipAndCullDistances";

    if (hasAngle)
    {
        GLint maxCullDistances = 0;
        glGetIntegerv(GL_MAX_CULL_DISTANCES_EXT, &maxCullDistances);
        if (maxCullDistances > 0)
        {
            validateErrorWithExt(GL_FRAGMENT_SHADER, "GL_ANGLE_clip_cull_distance", kFS, kExpect);
        }
    }

    if (hasExt)
    {
        validateErrorWithExt(GL_FRAGMENT_SHADER, "GL_EXT_clip_cull_distance", kFS, kExpect);
    }
}

// Shader redeclares gl_ClipDistance and gl_CullDistance
// But, the sum of the sizes is greater than gl_MaxCombinedClipAndCullDistances
TEST_P(GLSLValidationClipDistanceTest_ES3, TooManyCombinedFS2)
{
    const bool hasExt   = IsGLExtensionEnabled("GL_EXT_clip_cull_distance");
    const bool hasAngle = IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance");
    ANGLE_SKIP_TEST_IF(!hasExt && !hasAngle);

    GLint maxCombinedClipAndCullDistances = 0;
    glGetIntegerv(GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES_EXT, &maxCombinedClipAndCullDistances);
    ANGLE_SKIP_TEST_IF(maxCombinedClipAndCullDistances > 9);

    const char kFS[] = R"(in highp float gl_ClipDistance[5];
in highp float gl_CullDistance[4];

in highp vec4 aPosition;

out highp vec4 fragColor;

void main()
{
    fragColor.x = gl_ClipDistance[gl_MaxClipDistances - 6 + 1];
    fragColor.y = gl_ClipDistance[gl_MaxClipDistances - int(aPosition.x)];
    fragColor.z = gl_CullDistance[gl_MaxCullDistances - 6 + 1];
    fragColor.w = gl_CullDistance[gl_MaxCullDistances - int(aPosition.x)];
})";
    constexpr char kExpect[] =
        "The sum of 'gl_ClipDistance' and 'gl_CullDistance' size is greater than "
        "gl_MaxCombinedClipAndCullDistances";

    if (hasAngle)
    {
        GLint maxCullDistances = 0;
        glGetIntegerv(GL_MAX_CULL_DISTANCES_EXT, &maxCullDistances);
        if (maxCullDistances > 0)
        {
            validateErrorWithExt(GL_FRAGMENT_SHADER, "GL_ANGLE_clip_cull_distance", kFS, kExpect);
        }
    }

    if (hasExt)
    {
        validateErrorWithExt(GL_FRAGMENT_SHADER, "GL_EXT_clip_cull_distance", kFS, kExpect);
    }
}

// In fragment shader, writing to gl_ClipDistance should be denied.
TEST_P(GLSLValidationClipDistanceTest_ES3, FragmentWriteToClipDistance)
{
    const char kFS[] = R"(out highp vec4 fragColor;

void main()
{
    gl_ClipDistance[0] = 0.0f;
    fragColor = vec4(1, gl_ClipDistance[0], 0, 1);
})";
    constexpr char kExpect[] =
        "l-value required (can't modify gl_ClipDistance in a fragment shader \"gl_ClipDistance\")";

    if (IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance"))
    {
        validateErrorWithExt(GL_FRAGMENT_SHADER, "GL_ANGLE_clip_cull_distance", kFS, kExpect);
    }

    if (IsGLExtensionEnabled("GL_EXT_clip_cull_distance"))
    {
        validateErrorWithExt(GL_FRAGMENT_SHADER, "GL_EXT_clip_cull_distance", kFS, kExpect);
    }
}

// In fragment shader, writing to gl_CullDistance should be denied even if redeclaring it with the
// array size
TEST_P(GLSLValidationClipDistanceTest_ES3, FragmentWriteToCullDistance)
{
    const char kFS[] = R"(out highp vec4 fragColor;

in highp float gl_CullDistance[1];

void main()
{
    gl_CullDistance[0] = 0.0f;
    fragColor = vec4(1, gl_CullDistance[0], 0, 1);
})";
    constexpr char kExpect[] =
        "l-value required (can't modify gl_CullDistance in a fragment shader \"gl_CullDistance\")";

    if (IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance"))
    {
        validateErrorWithExt(GL_FRAGMENT_SHADER, "GL_ANGLE_clip_cull_distance", kFS, kExpect);
    }

    if (IsGLExtensionEnabled("GL_EXT_clip_cull_distance"))
    {
        validateErrorWithExt(GL_FRAGMENT_SHADER, "GL_EXT_clip_cull_distance", kFS, kExpect);
    }
}

// Accessing to gl_Clip/CullDistance with non-const index should be denied if the size of
// gl_Clip/CullDistance is not decided.
TEST_P(GLSLValidationClipDistanceTest_ES3, FragmentDynamicIndexWhenNotRedeclared)
{
    const char kFS[] = R"(out highp vec4 fragColor;

void main()
{
    mediump float color[3];
    for(int i = 0 ; i < 3 ; i++)
    {
        color[i] = gl_CullDistance[i];
    }
    fragColor = vec4(color[0], color[1], color[2], 1.0f);
})";
    constexpr char kExpect[] =
        "The gl_CullDistance array must be sized by the shader either redeclaring it with a size "
        "or indexing it only with constant integral expressions";

    if (IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance"))
    {
        validateErrorWithExt(GL_FRAGMENT_SHADER, "GL_ANGLE_clip_cull_distance", kFS, kExpect);
    }

    if (IsGLExtensionEnabled("GL_EXT_clip_cull_distance"))
    {
        validateErrorWithExt(GL_FRAGMENT_SHADER, "GL_EXT_clip_cull_distance", kFS, kExpect);
    }
}

// In compute shader, redeclaring gl_ClipDistance should be denied.
TEST_P(GLSLValidationClipDistanceTest_ES31, ComputeDeclareClipDistance)
{
    const char kCS[]         = R"(layout(local_size_x = 1) in;
highp float gl_ClipDistance[1];
void main() {})";
    constexpr char kExpect[] = "reserved built-in name";

    if (IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance"))
    {
        validateErrorWithExt(GL_COMPUTE_SHADER, "GL_ANGLE_clip_cull_distance", kCS, kExpect);
    }

    if (IsGLExtensionEnabled("GL_EXT_clip_cull_distance"))
    {
        validateErrorWithExt(GL_COMPUTE_SHADER, "GL_EXT_clip_cull_distance", kCS, kExpect);
    }
}

// In compute shader, writing to gl_ClipDistance should be denied.
TEST_P(GLSLValidationClipDistanceTest_ES31, ComputeWriteClipDistance)
{
    const char kCS[]         = R"(layout(local_size_x = 1) in;
void main() { gl_ClipDistance[0] = 1.0; })";
    constexpr char kExpect[] = "'gl_ClipDistance' : undeclared identifier";

    if (IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance"))
    {
        validateErrorWithExt(GL_COMPUTE_SHADER, "GL_ANGLE_clip_cull_distance", kCS, kExpect);
    }

    if (IsGLExtensionEnabled("GL_EXT_clip_cull_distance"))
    {
        validateErrorWithExt(GL_COMPUTE_SHADER, "GL_EXT_clip_cull_distance", kCS, kExpect);
    }
}

// In compute shader, reading gl_ClipDistance should be denied.
TEST_P(GLSLValidationClipDistanceTest_ES31, ComputeReadClipDistance)
{
    const char kCS[]         = R"(layout(local_size_x = 1) in;
void main() { highp float c = gl_ClipDistance[0]; })";
    constexpr char kExpect[] = "'gl_ClipDistance' : undeclared identifier";

    if (IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance"))
    {
        validateErrorWithExt(GL_COMPUTE_SHADER, "GL_ANGLE_clip_cull_distance", kCS, kExpect);
    }

    if (IsGLExtensionEnabled("GL_EXT_clip_cull_distance"))
    {
        validateErrorWithExt(GL_COMPUTE_SHADER, "GL_EXT_clip_cull_distance", kCS, kExpect);
    }
}

// In compute shader, redeclaring gl_CullDistance should be denied.
TEST_P(GLSLValidationClipDistanceTest_ES31, ComputeDeclareCullDistance)
{
    const char kCS[]         = R"(layout(local_size_x = 1) in;
highp float gl_CullDistance[1];
void main() {})";
    constexpr char kExpect[] = "reserved built-in name";

    if (IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance"))
    {
        validateErrorWithExt(GL_COMPUTE_SHADER, "GL_ANGLE_clip_cull_distance", kCS, kExpect);
    }

    if (IsGLExtensionEnabled("GL_EXT_clip_cull_distance"))
    {
        validateErrorWithExt(GL_COMPUTE_SHADER, "GL_EXT_clip_cull_distance", kCS, kExpect);
    }
}

// In compute shader, writing to gl_CullDistance should be denied.
TEST_P(GLSLValidationClipDistanceTest_ES31, ComputeWriteCullDistance)
{
    const char kCS[]         = R"(layout(local_size_x = 1) in;
void main() { gl_CullDistance[0] = 1.0; })";
    constexpr char kExpect[] = "'gl_CullDistance' : undeclared identifier";

    if (IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance"))
    {
        validateErrorWithExt(GL_COMPUTE_SHADER, "GL_ANGLE_clip_cull_distance", kCS, kExpect);
    }

    if (IsGLExtensionEnabled("GL_EXT_clip_cull_distance"))
    {
        validateErrorWithExt(GL_COMPUTE_SHADER, "GL_EXT_clip_cull_distance", kCS, kExpect);
    }
}

// In compute shader, reading gl_CullDistance should be denied.
TEST_P(GLSLValidationClipDistanceTest_ES31, ComputeReadCullDistance)
{
    const char kCS[]         = R"(layout(local_size_x = 1) in;
void main() { highp float c = gl_CullDistance[0]; })";
    constexpr char kExpect[] = "'gl_CullDistance' : undeclared identifier";

    if (IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance"))
    {
        validateErrorWithExt(GL_COMPUTE_SHADER, "GL_ANGLE_clip_cull_distance", kCS, kExpect);
    }

    if (IsGLExtensionEnabled("GL_EXT_clip_cull_distance"))
    {
        validateErrorWithExt(GL_COMPUTE_SHADER, "GL_EXT_clip_cull_distance", kCS, kExpect);
    }
}

class GLSLValidationTextureRectangleTest : public GLSLValidationTest
{};

// Check that if the extension is not supported, trying to use the features without having an
// extension directive fails.
//
// If the extension is supported, check that new types and builtins are usable even with the
// #extension directive
// Issue #15 of ARB_texture_rectangle explains that the extension was specified before the
// #extension mechanism was in place so it doesn't require explicit enabling.
TEST_P(GLSLValidationTextureRectangleTest, NewTypeAndBuiltinsWithoutExtensionDirective)
{
    const char kFS[] = R"(
precision mediump float;
uniform sampler2DRect tex;
void main()
{
    vec4 color = texture2DRect(tex, vec2(1.0));
    color = texture2DRectProj(tex, vec3(1.0));
    color = texture2DRectProj(tex, vec4(1.0));
})";
    if (IsGLExtensionEnabled("GL_ANGLE_texture_rectangle"))
    {
        validateSuccess(GL_FRAGMENT_SHADER, kFS);
    }
    else
    {
        validateError(GL_FRAGMENT_SHADER, kFS,
                      "'GL_ARB_texture_rectangle' : extension is not supported");
    }
}

// Check that if the extension is not supported, trying to use the features fails.
//
// If the extension is supported, test that using the feature with the extension directive passes.
TEST_P(GLSLValidationTextureRectangleTest, NewTypeAndBuiltinsWithExtensionDirective)
{
    const char kFS[] = R"(#extension GL_ARB_texture_rectangle : enable
precision mediump float;
uniform sampler2DRect tex;
void main()
{
    vec4 color = texture2DRect(tex, vec2(1.0));
    color = texture2DRectProj(tex, vec3(1.0));
    color = texture2DRectProj(tex, vec4(1.0));
})";
    if (IsGLExtensionEnabled("GL_ANGLE_texture_rectangle"))
    {
        validateSuccess(GL_FRAGMENT_SHADER, kFS);
    }
    else
    {
        validateError(GL_FRAGMENT_SHADER, kFS,
                      "'GL_ARB_texture_rectangle' : extension is not supported");
    }
}

// Check that it is not possible to pass a sampler2DRect where sampler2D is expected, and vice versa
TEST_P(GLSLValidationTextureRectangleTest, Rect2DVs2DMismatch)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_texture_rectangle"));

    {
        const char kFS[] = R"(
#extension GL_ARB_texture_rectangle : require
precision mediump float;
uniform sampler2DRect tex;
void main() {
    vec4 color = texture2D(tex, vec2(1.0));"
})";
        validateError(GL_FRAGMENT_SHADER, kFS,
                      "'texture2D' : no matching overloaded function found");
    }

    {
        const char kFS[] = R"(
#extension GL_ARB_texture_rectangle : require
precision mediump float;
uniform sampler2D tex;
void main() {
    vec4 color = texture2DRect(tex, vec2(1.0));"
})";
        validateError(GL_FRAGMENT_SHADER, kFS,
                      "'texture2DRect' : no matching overloaded function found");
    }
}

// Disabling ARB_texture_rectangle in GLSL should work, even if it is enabled by default.
// See ARB_texture_rectangle spec: "a shader can still include all variations of #extension
// GL_ARB_texture_rectangle in its source code"
TEST_P(GLSLValidationTextureRectangleTest, DisableARBTextureRectangle)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_texture_rectangle"));

    const char kFS[] = R"(#extension GL_ARB_texture_rectangle : disable
precision mediump float;

uniform sampler2DRect s;
void main()
{})";
    validateError(GL_FRAGMENT_SHADER, kFS, "'GL_ARB_texture_rectangle' : extension is disabled");
}

class GLSLValidationAtomicCounterTest_ES31 : public GLSLValidationTest_ES31
{};

// Test that ESSL 3.00 doesn't support atomic_uint.
TEST_P(GLSLValidationAtomicCounterTest_ES31, InvalidShaderVersion)
{
    constexpr char kFS[] = R"(#version 300 es
layout(binding = 0, offset = 4) uniform atomic_uint a;
void main()
{
})";
    validateError(GL_FRAGMENT_SHADER, kFS, "'atomic_uint' : Illegal use of reserved word");
}

// Test that any qualifier other than uniform leads to compile-time error.
TEST_P(GLSLValidationAtomicCounterTest_ES31, InvalidQualifier)
{
    constexpr char kCS[] = R"(#version 310 es
layout(local_size_x=1) in;
layout(binding = 0, offset = 4) in atomic_uint a;
void main()
{
})";
    validateError(GL_COMPUTE_SHADER, kCS, "'atomic_uint' : atomic_uints must be uniform");
}

// Test that uniform must be specified for declaration.
TEST_P(GLSLValidationAtomicCounterTest_ES31, UniformMustSpecifiedForDeclaration)
{
    constexpr char kCS[] = R"(#version 310 es
layout(local_size_x=1) in;
atomic_uint a;
void main()
{
})";
    validateError(GL_COMPUTE_SHADER, kCS, "'atomic_uint' : atomic_uints must be uniform");
}

// Test that offset overlapping leads to compile-time error (ESSL 3.10 section 4.4.6).
TEST_P(GLSLValidationAtomicCounterTest_ES31, BindingOffsetOverlapping)
{
    constexpr char kCS[] = R"(#version 310 es
layout(local_size_x=1) in;
layout(binding = 0, offset = 4) uniform atomic_uint a;
layout(binding = 0, offset = 6) uniform atomic_uint b;
void main()
{
})";
    validateError(GL_COMPUTE_SHADER, kCS, "'atomic counter' : Offset overlapping");
}

// Test offset inheritance for multiple variables in one same declaration.
TEST_P(GLSLValidationAtomicCounterTest_ES31, MultipleVariablesDeclaration)
{
    constexpr char kCS[] = R"(#version 310 es
layout(local_size_x=1) in;
layout(binding = 0, offset = 4) uniform atomic_uint a, b;
layout(binding = 0, offset = 8) uniform atomic_uint c;
void main()
{
})";
    validateError(GL_COMPUTE_SHADER, kCS, "'atomic counter' : Offset overlapping");
}

// Test that subsequent declarations inherit the globally specified offset.
TEST_P(GLSLValidationAtomicCounterTest_ES31, GlobalBindingOffsetOverlapping)
{
    constexpr char kCS[] = R"(#version 310 es
layout(local_size_x=1) in;
layout(binding = 2, offset = 4) uniform atomic_uint;
layout(binding = 2) uniform atomic_uint b;
layout(binding = 2, offset = 4) uniform atomic_uint c;
void main()
{
})";
    validateError(GL_COMPUTE_SHADER, kCS, "'atomic counter' : Offset overlapping");
}

// The spec only demands offset unique and non-overlapping. So this should be allowed.
TEST_P(GLSLValidationAtomicCounterTest_ES31, DeclarationSequenceWithDecrementalOffsetsSpecified)
{
    constexpr char kCS[] = R"(#version 310 es
layout(local_size_x=1) in;
layout(binding = 2, offset = 4) uniform atomic_uint a;
layout(binding = 2, offset = 0) uniform atomic_uint b;
void main()
{
})";
    validateSuccess(GL_COMPUTE_SHADER, kCS);
}

// Test that image format qualifiers are not allowed for atomic counters.
TEST_P(GLSLValidationAtomicCounterTest_ES31, ImageFormatMustNotSpecified)
{
    constexpr char kCS[] = R"(#version 310 es
layout(local_size_x=1) in;
layout(binding = 0, offset = 4, rgba32f) uniform atomic_uint a;
void main()
{
})";
    validateError(GL_COMPUTE_SHADER, kCS,
                  "'rgba32f' : invalid layout qualifier: only valid when used with images");
}

// Test that global layout qualifiers must not use 'offset'.
TEST_P(GLSLValidationAtomicCounterTest_ES31, OffsetMustNotSpecifiedForGlobalLayoutQualifier)
{
    constexpr char kCS[] = R"(#version 310 es
layout(local_size_x=1) in;
layout(offset = 4) in;
void main()
{
})";
    validateError(GL_COMPUTE_SHADER, kCS,
                  "'offset' : invalid layout qualifier: only valid when used with atomic counters");
}

// Test that offset overlapping leads to compile-time error (ESSL 3.10 section 4.4.6).
// Note that there is some vagueness in the spec when it comes to this test.
TEST_P(GLSLValidationAtomicCounterTest_ES31, BindingOffsetOverlappingForArrays)
{
    GLint maxAtomicCounterBuffers = 0;
    glGetIntegerv(GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS, &maxAtomicCounterBuffers);
    ANGLE_SKIP_TEST_IF(maxAtomicCounterBuffers < 3);

    constexpr char kCS[] = R"(#version 310 es
layout(local_size_x=1) in;
layout(binding = 2, offset = 4) uniform atomic_uint[2] a;
layout(binding = 2, offset = 8) uniform atomic_uint b;
void main()
{
})";
    validateError(GL_COMPUTE_SHADER, kCS, "'atomic counter' : Offset overlapping");
}

class GLSLValidationShaderStorageBlockTest_ES31 : public GLSLValidationTest_ES31
{};

// Test that shader storage block layout qualifiers can be declared for global scope.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, LayoutQualifiersDeclaredInGlobal)
{
    constexpr char kFS[] = R"(#version 310 es
layout(shared, column_major) buffer;
void main()
{
})";
    validateSuccess(GL_FRAGMENT_SHADER, kFS);
}

// Test that it is a compile-time error to declare buffer variables at global scope (outside a
// block).
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, DeclareBufferVariableAtGlobal)
{
    constexpr char kFS[] = R"(#version 310 es
layout(binding = 3) buffer int a;
void main()
{
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'buffer' : cannot declare buffer variables at global scope(outside a block)");
}

// Test that the buffer variable can't be opaque type.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, BufferVariableWithOpaqueType)
{
    constexpr char kFS[] = R"(#version 310 es
layout(binding = 3) buffer buf {
    int b1;
    atomic_uint b2;
};
void main()
{
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'buf' : Opaque types are not allowed in interface blocks");
}

// Test that the uniform variable can't be in shader storage block.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, UniformVariableInShaderStorageBlock)
{
    constexpr char kFS[] = R"(#version 310 es
layout(binding = 3) buffer buf {
    uniform int a;
};
void main()
{
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'uniform' : invalid qualifier on shader storage block member");
}

// Test that buffer qualifier is not supported in version lower than GLSL ES 3.10.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, BufferQualifierInESSL3)
{
    constexpr char kFS[] = R"(#version 300 es
layout(binding = 3) buffer buf {
    int b1;
    buffer int b2;
};
void main()
{
})";
    validateError(GL_FRAGMENT_SHADER, kFS, "'binding' : invalid layout qualifier: not supported");
}

// Test that can't assign to a readonly buffer variable.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, AssignToReadonlyBufferVariable)
{
    constexpr char kFS[] = R"(#version 310 es
layout(binding = 3) buffer buf {
    readonly int b1;
};
void main()
{
    b1 = 5;
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  R"('assign' : l-value required (can't modify a readonly variable "b1"))");
}

// Test that can't assign to a buffer variable declared within shader storage block with readonly.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, AssignToBufferVariableWithinReadonlyBlock)
{
    constexpr char kFS[] = R"(#version 310 es
layout(binding = 3) readonly buffer buf {
    int b1;
};
void main()
{
    b1 = 5;
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  R"('assign' : l-value required (can't modify a readonly variable "b1"))");
}

// Test that can't assign to a readonly buffer variable through an instance name.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, AssignToReadonlyBufferVariableByInstanceName)
{
    constexpr char kFS[] = R"(#version 310 es
precision highp float;
layout(binding = 3) buffer buf {
    readonly float f;
} instanceBuffer;
void main()
{
    instanceBuffer.f += 0.2;
})";
    validateError(GL_FRAGMENT_SHADER, kFS, "'assign' : can't modify a readonly variable");
}

// Test that can't assign to a readonly struct buffer variable.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, AssignToReadonlyStructBufferVariable)
{
    constexpr char kFS[] = R"(#version 310 es
precision highp float;
struct S {
    float f;
};
layout(binding = 3) buffer buf {
    readonly S s;
};
void main()
{
    s.f += 0.2;
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  R"('assign' : l-value required (can't modify a readonly variable "s"))");
}

// Test that can't assign to a readonly struct buffer variable through an instance name.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31,
       AssignToReadonlyStructBufferVariableByInstanceName)
{
    constexpr char kFS[] = R"(#version 310 es
precision highp float;
struct S {
    float f;
};
layout(binding = 3) buffer buf {
    readonly S s;
} instanceBuffer;
void main()
{
    instanceBuffer.s.f += 0.2;
})";
    validateError(GL_FRAGMENT_SHADER, kFS, "'assign' : can't modify a readonly variable");
}

// Test that a readonly and writeonly buffer variable should neither read or write.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, AccessReadonlyWriteonlyBufferVariable)
{
    constexpr char kFS[] = R"(#version 310 es
layout(binding = 3) buffer buf {
    readonly writeonly int b1;
};
void main()
{
    b1 = 5;
    int test = b1;
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  R"('assign' : l-value required (can't modify a readonly variable "b1"))");
}

// Test that accessing a writeonly buffer variable should be error.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, AccessWriteonlyBufferVariable)
{
    constexpr char kFS[] = R"(#version 310 es
layout(binding = 3) buffer buf {
    writeonly int b1;
};
void main()
{
    int test = b1;
})";
    validateError(GL_FRAGMENT_SHADER, kFS, "'=' : Invalid operation for variables with writeonly");
}

// Test that accessing a buffer variable through an instance name inherits the writeonly qualifier
// and generates errors.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, AccessWriteonlyBufferVariableByInstanceName)
{
    constexpr char kFS[] = R"(#version 310 es
precision highp float;
layout(binding = 3) writeonly buffer buf {
    float f;
} instanceBuffer;
void main()
{
    float test = instanceBuffer.f;
})";
    validateError(GL_FRAGMENT_SHADER, kFS, "'=' : Invalid operation for variables with writeonly");
}

// Test that writeonly buffer variable as the argument of a unary operator should be error.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, UnaryOperatorWithWriteonlyBufferVariable)
{
    constexpr char kFS[] = R"(#version 310 es
layout(binding = 3) buffer buf {
    writeonly int b1;
};
void main()
{
    ++b1;
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'++' : wrong operand type - no operation '++' exists that takes an operand of "
                  "type buffer mediump writeonly int (or there is no acceptable conversion)");
}

// Test that writeonly buffer variable on the left-hand side of compound assignment should be error.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, CompoundAssignmentToWriteonlyBufferVariable)
{
    constexpr char kFS[] = R"(#version 310 es
layout(binding = 3) buffer buf {
    writeonly int b1;
};
void main()
{
    b1 += 5;
})";
    validateError(GL_FRAGMENT_SHADER, kFS, "'+=' : Invalid operation for variables with writeonly");
}

// Test that writeonly buffer variable as ternary op argument should be error.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, TernarySelectionWithWriteonlyBufferVariable)
{
    constexpr char kFS[] = R"(#version 310 es
layout(binding = 3) buffer buf {
    writeonly bool b1;
};
void main()
{
    int test = b1 ? 1 : 0;
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'?:' : ternary operator is not allowed for variables with writeonly");
}

// Test that writeonly buffer variable as array constructor argument should be error.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, ArrayConstructorWithWriteonlyBufferVariable)
{
    constexpr char kFS[] = R"(#version 310 es
precision highp float;
layout(binding = 3) buffer buf {
    writeonly float f;
};
void main()
{
    float a[3] = float[3](f, f, f);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'constructor' : cannot convert a variable with writeonly");
}

// Test that writeonly buffer variable as structure constructor argument should be error.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, StructureConstructorWithWriteonlyBufferVariable)
{
    constexpr char kFS[] = R"(#version 310 es
struct S {
    int a;
};
struct T {
    S b;
};
layout(binding = 3) buffer buf {
    writeonly S c;
};
void main()
{
    T t = T(c);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'constructor' : cannot convert a variable with writeonly");
}

// Test that writeonly buffer variable as built-in function argument should be error.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, BuiltInFunctionWithWriteonlyBufferVariable)
{
    constexpr char kFS[] = R"(#version 310 es
layout(binding = 3) buffer buf {
    writeonly int a;
};
void main()
{
    int test = min(a, 1);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'min' : Writeonly value cannot be passed for 'in' or 'inout' parameters");
}

// Test that writeonly buffer variable as user-defined function in argument should be error.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31,
       UserDefinedFunctionWithWriteonlyBufferVariableInArgument)
{
    constexpr char kFS[] = R"(#version 310 es
precision highp float;
layout(binding = 3) buffer buf {
    writeonly float f;
};
void foo(float a) {}
void main()
{
    foo(f);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "foo' : Writeonly value cannot be passed for 'in' or 'inout' parameters");
}

// Test that readonly buffer variable as user-defined function out argument should be error.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31,
       UserDefinedFunctionWithReadonlyBufferVariableOutArgument)
{
    constexpr char kFS[] = R"(#version 310 es
precision highp float;
layout(binding = 3) buffer buf {
    readonly float f;
};
void foo(out float a) {}
void main()
{
    foo(f);
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  R"('assign' : l-value required (can't modify a readonly variable "f"))");
}

// Test that buffer qualifier can't modify a function parameter.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, BufferQualifierOnFunctionParameter)
{
    constexpr char kFS[] = R"(#version 310 es
precision highp float;
void foo(buffer float a) {}
void main()
{
})";
    validateError(GL_FRAGMENT_SHADER, kFS, "'buffer' : only allowed at global scope");
}

// Test that using std430 qualifier on a uniform block will fail to compile.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, UniformBlockWithStd430)
{
    constexpr char kFS[] = R"(#version 310 es
layout(std430) uniform buf {
    int b1;
    int b2;
};
void main()
{
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'std430' : The std430 layout is supported only for shader storage blocks");
}

// Test that indexing a runtime-sized array with a negative constant index does not compile.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, IndexRuntimeSizedArrayWithNegativeIndex)
{
    constexpr char kFS[] = R"(#version 310 es
layout(std430) buffer buf
{
    int arr[];
};

void main()
{
    arr[-1];
})";
    validateError(GL_FRAGMENT_SHADER, kFS, "'[]' : index expression is negative");
}

// Test that only the last member of a buffer can be runtime-sized.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, RuntimeSizedVariableInNotLastInBuffer)
{
    constexpr char kFS[] = R"(#version 310 es
layout(std430) buffer buf
{
    int arr[];
    int i;
};

void main()
{
})";
    validateError(GL_FRAGMENT_SHADER, kFS,
                  "'arr' : array members of interface blocks must specify a size");
}

// Test that memory qualifiers are output.
TEST_P(GLSLValidationShaderStorageBlockTest_ES31, MemoryQualifiers)
{
    constexpr char kFS[]         = R"(#version 310 es
precision highp float;
precision highp int;
layout(std430) coherent buffer buf
{
    int defaultCoherent;
    coherent ivec2 specifiedCoherent;
    volatile ivec3 specifiedVolatile;
    restrict ivec4 specifiedRestrict;
    readonly float specifiedReadOnly;
    writeonly vec2 specifiedWriteOnly;
    volatile readonly vec3 specifiedMultiple;
};

void main()
{
})";
    const CompiledShader &shader = compile(GL_FRAGMENT_SHADER, kFS);
    EXPECT_TRUE(shader.success());
    if (IsOpenGLES())
    {
        // The following are GLSL qualifiers, so only valid with GLSL translation.
        EXPECT_TRUE(shader.verifyInTranslatedSource("coherent highp int"));
        EXPECT_TRUE(shader.verifyInTranslatedSource("coherent highp ivec2"));
        EXPECT_TRUE(shader.verifyInTranslatedSource("coherent volatile highp ivec3"));
        EXPECT_TRUE(shader.verifyInTranslatedSource("coherent restrict highp ivec4"));
        EXPECT_TRUE(shader.verifyInTranslatedSource("readonly coherent highp float"));
        EXPECT_TRUE(shader.verifyInTranslatedSource("writeonly coherent highp vec2"));
        EXPECT_TRUE(shader.verifyInTranslatedSource("readonly coherent volatile highp vec3"));
    }
    else if (IsOpenGL())
    {
        // The following are GLSL qualifiers, so only valid with GLSL translation.
        EXPECT_TRUE(shader.verifyInTranslatedSource("coherent int"));
        EXPECT_TRUE(shader.verifyInTranslatedSource("coherent ivec2"));
        EXPECT_TRUE(shader.verifyInTranslatedSource("coherent volatile ivec3"));
        EXPECT_TRUE(shader.verifyInTranslatedSource("coherent restrict ivec4"));
        EXPECT_TRUE(shader.verifyInTranslatedSource("readonly coherent float"));
        EXPECT_TRUE(shader.verifyInTranslatedSource("writeonly coherent vec2"));
        EXPECT_TRUE(shader.verifyInTranslatedSource("readonly coherent volatile vec3"));
    }
    reset();
}

class GLSLValidationBaseVertexTest_ES3 : public GLSLValidationTest_ES3
{};

class WebGL2GLSLValidationBaseVertexTest : public WebGL2GLSLValidationTest
{};

// Check that base vertex/instance is not exposed to WebGL.
TEST_P(WebGL2GLSLValidationBaseVertexTest, NoSupport)
{
    constexpr char kVS[] = R"(#version 300 es
#extension GL_ANGLE_base_vertex_base_instance_shader_builtin : require
void main() {
   gl_Position = vec4(float(gl_BaseVertex), float(gl_BaseInstance), 0.0, 1.0);
})";
    validateError(
        GL_VERTEX_SHADER, kVS,
        "'GL_ANGLE_base_vertex_base_instance_shader_builtin' : extension is not supported");
}

// Check that compiling with the old extension doesn't work
TEST_P(GLSLValidationBaseVertexTest_ES3, CheckCompileOldExtension)
{
    constexpr char kVS[] = R"(#version 300 es
#extension GL_ANGLE_base_vertex_base_instance : require
void main() {
   gl_Position = vec4(float(gl_BaseVertex), float(gl_BaseInstance), 0.0, 1.0);
})";
    validateError(GL_VERTEX_SHADER, kVS,
                  "'GL_ANGLE_base_vertex_base_instance' : extension is not supported");
}

// Check that a user-defined "gl_BaseVertex" or "gl_BaseInstance" is not permitted
TEST_P(GLSLValidationBaseVertexTest_ES3, DisallowsUserDefinedGLDrawID)
{
    {
        // Check that it is not permitted without the
        // GL_ANGLE_base_vertex_base_instance_shader_builtin extension
        constexpr char kVS[] = R"(#version 300 es
uniform int gl_BaseVertex;
void main() {
   gl_Position = vec4(float(gl_BaseVertex), 0.0, 0.0, 1.0);
})";
        validateError(GL_VERTEX_SHADER, kVS, "'gl_' : reserved built-in name");
    }

    {
        constexpr char kVS[] = R"(#version 300 es
uniform int gl_BaseInstance;
void main() {
   gl_Position = vec4(float(gl_BaseInstance), 0.0, 0.0, 1.0);
})";
        validateError(GL_VERTEX_SHADER, kVS, "'gl_' : reserved built-in name");
    }

    {
        constexpr char kVS[] = R"(#version 300 es
void main() {
   int gl_BaseVertex = 0;
   gl_Position = vec4(float(gl_BaseVertex), 0.0, 0.0, 1.0);
})";
        validateError(GL_VERTEX_SHADER, kVS, "'gl_' : reserved built-in name");
    }

    {
        constexpr char kVS[] = R"(#version 300 es
void main() {
   int gl_BaseInstance = 0;
   gl_Position = vec4(float(gl_BaseInstance), 0.0, 0.0, 1.0);
})";
        validateError(GL_VERTEX_SHADER, kVS, "'gl_' : reserved built-in name");
    }

    {
        // Check that it is not permitted with the extension
        constexpr char kVS[] = R"(#version 300 es
#extension GL_ANGLE_base_vertex_base_instance_shader_builtin : require
uniform int gl_BaseVertex;
void main() {
   gl_Position = vec4(float(gl_BaseVertex), 0.0, 0.0, 1.0);
})";
        validateError(GL_VERTEX_SHADER, kVS, "'gl_' : reserved built-in name");
    }

    {
        constexpr char kVS[] = R"(#version 300 es
#extension GL_ANGLE_base_vertex_base_instance_shader_builtin : require
uniform int gl_BaseInstance;
void main() {
   gl_Position = vec4(float(gl_BaseInstance), 0.0, 0.0, 1.0);
})";
        validateError(GL_VERTEX_SHADER, kVS, "'gl_' : reserved built-in name");
    }

    {
        constexpr char kVS[] = R"(#version 300 es
#extension GL_ANGLE_base_vertex_base_instance_shader_builtin : require
void main() {
   int gl_BaseVertex = 0;
   gl_Position = vec4(float(gl_BaseVertex), 0.0, 0.0, 1.0);
})";
        validateError(GL_VERTEX_SHADER, kVS, "'gl_' : reserved built-in name");
    }

    {
        constexpr char kVS[] = R"(#version 300 es
#extension GL_ANGLE_base_vertex_base_instance_shader_builtin : require
void main() {
   int gl_BaseInstance = 0;
   gl_Position = vec4(float(gl_BaseInstance), 0.0, 0.0, 1.0);
})";
        validateError(GL_VERTEX_SHADER, kVS, "'gl_' : reserved built-in name");
    }
}

class GLSLValidationDrawIDTest : public GLSLValidationTest
{};

// Check that a user-defined "gl_DrawID" is not permitted
TEST_P(GLSLValidationDrawIDTest, DisallowsUserDefinedGLDrawID)
{
    {
        // Check that it is not permitted without the GL_ANGLE_multi_draw extension
        constexpr char kVS[] = R"(uniform int gl_DrawID;
void main() {
   gl_Position = vec4(float(gl_DrawID), 0.0, 0.0, 1.0);
})";
        validateError(GL_VERTEX_SHADER, kVS, "'gl_' : reserved built-in name");
    }

    {
        constexpr char kVS[] = R"(void main() {
   int gl_DrawID = 0;
   gl_Position = vec4(float(gl_DrawID), 0.0, 0.0, 1.0);
})";
        validateError(GL_VERTEX_SHADER, kVS, "'gl_' : reserved built-in name");
    }

    {
        // Check that it is not permitted with the extension
        constexpr char kVS[] = R"(#extension GL_ANGLE_multi_draw : require
uniform int gl_DrawID;
void main() {
   gl_Position = vec4(float(gl_DrawID), 0.0, 0.0, 1.0);
})";
        validateError(GL_VERTEX_SHADER, kVS, "'gl_' : reserved built-in name");
    }

    {
        constexpr char kVS[] = R"(#extension GL_ANGLE_multi_draw : require
void main() {
   int gl_DrawID = 0;
   gl_Position = vec4(float(gl_DrawID), 0.0, 0.0, 1.0);
})";
        validateError(GL_VERTEX_SHADER, kVS, "'gl_' : reserved built-in name");
    }
}

class GLSLValidationExtensionDirectiveTest_ES3 : public GLSLValidationTest_ES3
{
  public:
    void testCompileNeedsExtensionDirective(GLenum shaderType,
                                            const char *shaderSource,
                                            const char *version,
                                            const char *extension,
                                            bool isExtensionSupported,
                                            const char *expectWithoutPragma,
                                            const char *expectWithExtDisabled)
    {
        testCompileNeedsExtensionDirectiveImpl(shaderType, shaderSource, version, extension,
                                               isExtensionSupported, true, expectWithoutPragma,
                                               expectWithExtDisabled);
    }

    void testCompileNeedsExtensionDirectiveGenericKeyword(GLenum shaderType,
                                                          const char *shaderSource,
                                                          const char *version,
                                                          const char *extension,
                                                          bool isExtensionSupported,
                                                          const char *expect)
    {
        testCompileNeedsExtensionDirectiveImpl(shaderType, shaderSource, version, extension,
                                               isExtensionSupported, false, expect, expect);
    }

    void testCompileNeedsExtensionDirectiveImpl(GLenum shaderType,
                                                const char *shaderSource,
                                                const char *version,
                                                const char *extension,
                                                bool isExtensionSupported,
                                                bool willWarnOnUse,
                                                const char *expectWithoutPragma,
                                                const char *expectWithExtDisabled)
    {
        {
            std::stringstream src;
            if (version)
            {
                src << version << "\n";
            }
            src << shaderSource;
            const CompiledShader &shader = compile(shaderType, src.str().c_str());
            EXPECT_FALSE(shader.success());
            EXPECT_TRUE(shader.hasInfoLog(expectWithoutPragma));
            reset();
        }

        {
            std::stringstream src;
            if (version)
            {
                src << version << "\n";
            }
            src << "#extension " << extension << ": disable\n" << shaderSource;
            const CompiledShader &shader = compile(shaderType, src.str().c_str());
            EXPECT_FALSE(shader.success());
            EXPECT_TRUE(shader.hasInfoLog(expectWithExtDisabled));
            reset();
        }

        {
            std::stringstream src;
            if (version)
            {
                src << version << "\n";
            }
            src << "#extension " << extension << ": enable\n" << shaderSource;
            if (isExtensionSupported)
            {
                EXPECT_TRUE(compile(shaderType, src.str().c_str()).success());
            }
            else
            {
                const CompiledShader &shader = compile(shaderType, src.str().c_str());
                EXPECT_FALSE(shader.success());
                EXPECT_TRUE(shader.hasInfoLog("extension is not supported"));
            }
            reset();
        }

        // The Nvidia/GLES driver doesn't treat warn like enable and gives an error, declaring that
        // using a token from the extension needs `#extension EXT: enable`.  Don't run these tests
        // on that config.
        const bool driverMishandlesWarn = IsOpenGLES() && IsNVIDIA();

        if (!driverMishandlesWarn)
        {
            std::stringstream src;
            if (version)
            {
                src << version << "\n";
            }
            src << "#extension " << extension << ": warn\n" << shaderSource;
            const CompiledShader &shader = compile(shaderType, src.str().c_str());
            if (!isExtensionSupported)
            {
                EXPECT_FALSE(shader.success());
                EXPECT_TRUE(shader.hasInfoLog("extension is not supported"));
            }
            else
            {
                EXPECT_TRUE(shader.success());
                if (willWarnOnUse)
                {
                    EXPECT_TRUE(shader.hasInfoLog("WARNING"));
                    EXPECT_TRUE(shader.hasInfoLog("extension is being used"));
                }
            }
            reset();
        }
    }
};

class GLSLValidationExtensionDirectiveTest_ES31 : public GLSLValidationExtensionDirectiveTest_ES3
{};

// OES_EGL_image_external needs to be enabled in GLSL to be able to use samplerExternalOES.
TEST_P(GLSLValidationExtensionDirectiveTest_ES3, SamplerExternalOESWithImage)
{
    const bool hasExt    = IsGLExtensionEnabled("GL_OES_EGL_image_external");
    const bool hasAnyExt = hasExt || IsGLExtensionEnabled("GL_NV_EGL_stream_consumer_external");

    constexpr char kFS[] = R"(precision mediump float;
uniform samplerExternalOES s;
void main()
{})";
    testCompileNeedsExtensionDirective(
        GL_FRAGMENT_SHADER, kFS, nullptr, "GL_OES_EGL_image_external", hasExt,
        hasAnyExt ? "extension is disabled" : "extension is not supported",
        hasAnyExt ? "extension is disabled" : "extension is not supported");
}

// NV_EGL_stream_consumer_external needs to be enabled in GLSL to be able to use samplerExternalOES.
TEST_P(GLSLValidationExtensionDirectiveTest_ES3, SamplerExternalOESWithStreamConstumer)
{
    const bool hasExt    = IsGLExtensionEnabled("GL_NV_EGL_stream_consumer_external");
    const bool hasAnyExt = hasExt || IsGLExtensionEnabled("GL_OES_EGL_image_external");

    constexpr char kFS[] = R"(precision mediump float;
uniform samplerExternalOES s;
void main()
{})";
    testCompileNeedsExtensionDirective(
        GL_FRAGMENT_SHADER, kFS, nullptr, "GL_NV_EGL_stream_consumer_external", hasExt,
        hasAnyExt ? "extension is disabled" : "extension is not supported",
        hasAnyExt ? "extension is disabled" : "extension is not supported");
}

// GL_EXT_YUV_target needs to be enabled in GLSL to be able to use samplerExternal2DY2YEXT.
TEST_P(GLSLValidationExtensionDirectiveTest_ES3, SamplerExternal2DY2YEXT)
{
    const bool hasExt = IsGLExtensionEnabled("GL_EXT_YUV_target");

    constexpr char kFS[] = R"(precision mediump float;
uniform __samplerExternal2DY2YEXT s;
void main()
{})";
    // __samplerExternal2DY2YEXT is not a reserved keyword, and the translator fails with syntax
    // error if extension is not specified.
    testCompileNeedsExtensionDirective(
        GL_FRAGMENT_SHADER, kFS, "#version 300 es", "GL_EXT_YUV_target", hasExt,
        "'s' : syntax error", hasExt ? "'s' : syntax error" : "extension is not supported");
}

// GL_EXT_YUV_target needs to be enabled in GLSL to be able to use layout(yuv).
TEST_P(GLSLValidationExtensionDirectiveTest_ES3, YUVLayoutNeedsExtensionDirective)
{
    const bool hasExt = IsGLExtensionEnabled("GL_EXT_YUV_target");

    constexpr char kFS[] = R"(precision mediump float;
layout(yuv) out vec4 color;
void main()
{})";
    testCompileNeedsExtensionDirective(
        GL_FRAGMENT_SHADER, kFS, "#version 300 es", "GL_EXT_YUV_target", hasExt,
        hasExt ? "extension is disabled" : "extension is not supported",
        hasExt ? "extension is disabled" : "extension is not supported");
}

// GL_EXT_blend_func_extended needs to be enabled in GLSL to be able to use
// gl_MaxDualSourceDrawBuffersEXT.
TEST_P(GLSLValidationExtensionDirectiveTest_ES3, MaxDualSourceDrawBuffersNeedsExtensionDirective)
{
    const bool hasExt = IsGLExtensionEnabled("GL_EXT_blend_func_extended");

    {
        constexpr char kFS[] = R"(precision mediump float;
void main() {
    gl_FragColor = vec4(gl_MaxDualSourceDrawBuffersEXT / 10);
})";
        testCompileNeedsExtensionDirective(
            GL_FRAGMENT_SHADER, kFS, nullptr, "GL_EXT_blend_func_extended", hasExt,
            hasExt ? "extension is disabled"
                   : "'gl_MaxDualSourceDrawBuffersEXT' : undeclared identifier",
            hasExt ? "extension is disabled"
                   : "'gl_MaxDualSourceDrawBuffersEXT' : undeclared identifier");
    }

    {
        constexpr char kFS[] = R"(precision mediump float;
layout(location = 0) out mediump vec4 fragColor;
void main() {
    fragColor = vec4(gl_MaxDualSourceDrawBuffersEXT / 10);
})";
        testCompileNeedsExtensionDirective(
            GL_FRAGMENT_SHADER, kFS, "#version 300 es", "GL_EXT_blend_func_extended", hasExt,
            hasExt ? "extension is disabled"
                   : "'gl_MaxDualSourceDrawBuffersEXT' : undeclared identifier",
            hasExt ? "extension is disabled"
                   : "'gl_MaxDualSourceDrawBuffersEXT' : undeclared identifier");
    }
}

// GL_EXT_clip_cull_distance or GL_ANGLE_clip_cull_distance needs to be enabled in GLSL to be able
// to use gl_ClipDistance and gl_CullDistance.
TEST_P(GLSLValidationExtensionDirectiveTest_ES3, ClipCullDistanceNeedsExtensionDirective)
{
    const bool hasExt   = IsGLExtensionEnabled("GL_EXT_clip_cull_distance");
    const bool hasAngle = IsGLExtensionEnabled("GL_ANGLE_clip_cull_distance");

    GLint maxClipDistances = 0;
    GLint maxCullDistances = 0;
    if (hasExt || hasAngle)
    {
        glGetIntegerv(GL_MAX_CLIP_DISTANCES_EXT, &maxClipDistances);
        EXPECT_GE(maxClipDistances, 8);

        glGetIntegerv(GL_MAX_CULL_DISTANCES_EXT, &maxCullDistances);
        EXPECT_TRUE(maxCullDistances == 0 || maxCullDistances >= 8);
        if (hasExt)
        {
            EXPECT_GE(maxCullDistances, 8);
        }
    }

    constexpr char kVS1[] = R"(uniform vec4 uPlane;
in vec4 aPosition;

void main()
{
    gl_Position = aPosition;
    gl_ClipDistance[0] = dot(aPosition, uPlane);
    gl_CullDistance[0] = dot(aPosition, uPlane);
})";

    constexpr char kVS2[] = R"(uniform vec4 uPlane;
in vec4 aPosition;

out highp float gl_ClipDistance[4];
out highp float gl_CullDistance[4];

void main()
{
    gl_Position = aPosition;
    gl_ClipDistance[gl_MaxClipDistances - 6 + 1] = dot(aPosition, uPlane);
    gl_ClipDistance[gl_MaxClipDistances - int(aPosition.x)] = dot(aPosition, uPlane);
    gl_ClipDistance[gl_MaxCombinedClipAndCullDistances / 4 - 1] = dot(aPosition, uPlane);
    gl_CullDistance[gl_MaxCullDistances - 6 + 1] = dot(aPosition, uPlane);
    gl_CullDistance[gl_MaxCullDistances - int(aPosition.x)] = dot(aPosition, uPlane);
})";

    // Shader using gl_ClipDistance and gl_CullDistance
    constexpr char kFS1[] = R"(out highp vec4 fragColor;
void main()
{
    fragColor = vec4(gl_ClipDistance[0], gl_CullDistance[0], 0, 1);
})";

    // Shader redeclares gl_ClipDistance and gl_CullDistance
    constexpr char kFS2[] = R"(in highp float gl_ClipDistance[4];
in highp float gl_CullDistance[4];
in highp vec4 aPosition;

out highp vec4 fragColor;

void main()
{
    fragColor.x = gl_ClipDistance[gl_MaxClipDistances - 6 + 1];
    fragColor.y = gl_ClipDistance[gl_MaxClipDistances - int(aPosition.x)];
    fragColor.z = gl_CullDistance[gl_MaxCullDistances - 6 + 1];
    fragColor.w = gl_CullDistance[gl_MaxCullDistances - int(aPosition.x)];
    fragColor *= gl_CullDistance[gl_MaxCombinedClipAndCullDistances / 4 - 1];
})";

    if (hasExt)
    {
        const char *expectWithoutPragma =
            hasExt ? "extension is disabled" : "extension is not supported";
        const char *expectWithExtDisabled =
            hasExt ? "extension is disabled" : "extension is not supported";

        testCompileNeedsExtensionDirective(GL_VERTEX_SHADER, kVS1, "#version 300 es",
                                           "GL_EXT_clip_cull_distance", hasExt, expectWithoutPragma,
                                           expectWithExtDisabled);
        testCompileNeedsExtensionDirective(GL_VERTEX_SHADER, kVS2, "#version 300 es",
                                           "GL_EXT_clip_cull_distance", hasExt, expectWithoutPragma,
                                           expectWithExtDisabled);
        testCompileNeedsExtensionDirective(GL_FRAGMENT_SHADER, kFS1, "#version 300 es",
                                           "GL_EXT_clip_cull_distance", hasExt, expectWithoutPragma,
                                           expectWithExtDisabled);
        testCompileNeedsExtensionDirective(GL_FRAGMENT_SHADER, kFS2, "#version 300 es",
                                           "GL_EXT_clip_cull_distance", hasExt, expectWithoutPragma,
                                           expectWithExtDisabled);
    }

    if (hasAngle && maxCullDistances > 0)
    {
        const char *expectWithoutPragma =
            hasAngle ? "extension is disabled" : "extension is not supported";
        const char *expectWithExtDisabled =
            hasAngle ? "extension is disabled" : "extension is not supported";

        testCompileNeedsExtensionDirective(GL_VERTEX_SHADER, kVS1, "#version 300 es",
                                           "GL_ANGLE_clip_cull_distance", hasAngle,
                                           expectWithoutPragma, expectWithExtDisabled);
        testCompileNeedsExtensionDirective(GL_VERTEX_SHADER, kVS2, "#version 300 es",
                                           "GL_ANGLE_clip_cull_distance", hasAngle,
                                           expectWithoutPragma, expectWithExtDisabled);
        testCompileNeedsExtensionDirective(GL_FRAGMENT_SHADER, kFS1, "#version 300 es",
                                           "GL_ANGLE_clip_cull_distance", hasAngle,
                                           expectWithoutPragma, expectWithExtDisabled);
        testCompileNeedsExtensionDirective(GL_FRAGMENT_SHADER, kFS2, "#version 300 es",
                                           "GL_ANGLE_clip_cull_distance", hasAngle,
                                           expectWithoutPragma, expectWithExtDisabled);
    }
}

// GL_EXT_frag_depth needs to be enabled in GLSL 100 to be able to use gl_FragDepthEXT.
TEST_P(GLSLValidationExtensionDirectiveTest_ES3, FragDepth)
{
    const bool hasExt = IsGLExtensionEnabled("GL_EXT_frag_depth");

    constexpr char kFS[] = R"(precision mediump float;
void main()
{
    gl_FragDepthEXT = 1.0;
})";
    testCompileNeedsExtensionDirective(
        GL_FRAGMENT_SHADER, kFS, nullptr, "GL_EXT_frag_depth", hasExt,
        hasExt ? "extension is disabled" : "'gl_FragDepthEXT' : undeclared identifier",
        hasExt ? "extension is disabled" : "extension is not supported");
}

// GL_EXT_shader_framebuffer_fetch or GL_EXT_shader_framebuffer_fetch_non_coherent needs to be
// enabled in GLSL 100 to be able to use gl_LastFragData and in GLSL 300+ to use inout.
TEST_P(GLSLValidationExtensionDirectiveTest_ES3, LastFragData)
{
    const bool hasCoherent = IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch");
    const bool hasNonCoherent =
        IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent");

    const char kFS100Coherent[] = R"(
uniform highp vec4 u_color;
highp vec4 gl_LastFragData[gl_MaxDrawBuffers];

void main (void)
{
    gl_FragColor = u_color + gl_LastFragData[0] + gl_LastFragData[2];
})";

    const char kFS300Coherent[] = R"(
inout highp vec4 o_color;
uniform highp vec4 u_color;

void main (void)
{
    o_color = clamp(o_color + u_color, vec4(0.0f), vec4(1.0f));
})";

    const char kFS100NonCoherent[] = R"(
uniform highp vec4 u_color;
layout(noncoherent) highp vec4 gl_LastFragData[gl_MaxDrawBuffers];

void main (void)
{
    gl_FragColor = u_color + gl_LastFragData[0] + gl_LastFragData[2];
})";

    const char kFS300NonCoherent[] = R"(
layout(noncoherent, location = 0) inout highp vec4 o_color;
uniform highp vec4 u_color;

void main (void)
{
    o_color = clamp(o_color + u_color, vec4(0.0f), vec4(1.0f));
})";

    testCompileNeedsExtensionDirective(
        GL_FRAGMENT_SHADER, kFS100Coherent, nullptr, "GL_EXT_shader_framebuffer_fetch", hasCoherent,
        hasCoherent ? hasNonCoherent ? "extension is disabled" : "extension is not supported"
                    : "'gl_' : reserved built-in name",
        hasCoherent ? hasNonCoherent ? "extension is disabled" : "extension is not supported"
                    : "extension is not supported");
    testCompileNeedsExtensionDirectiveGenericKeyword(
        GL_FRAGMENT_SHADER, kFS300Coherent, "#version 300 es", "GL_EXT_shader_framebuffer_fetch",
        hasCoherent, "'inout' : invalid qualifier");

    testCompileNeedsExtensionDirectiveGenericKeyword(GL_FRAGMENT_SHADER, kFS100NonCoherent, nullptr,
                                                     "GL_EXT_shader_framebuffer_fetch_non_coherent",
                                                     hasNonCoherent, "'layout' : syntax error");
    testCompileNeedsExtensionDirectiveGenericKeyword(GL_FRAGMENT_SHADER, kFS300NonCoherent,
                                                     "#version 300 es",
                                                     "GL_EXT_shader_framebuffer_fetch_non_coherent",
                                                     hasNonCoherent, "'inout' : invalid qualifier");
}

// GL_EXT_shader_texture_lod needs to be enabled to be able to use texture2DLodEXT.
TEST_P(GLSLValidationExtensionDirectiveTest_ES3, Texture2DLod)
{
    const bool hasExt = IsGLExtensionEnabled("GL_EXT_shader_texture_lod");

    constexpr char kFS[] = R"(precision mediump float;
varying vec2 texCoord0v;
uniform float lod;
uniform sampler2D tex;
void main()
{
    vec4 color = texture2DLodEXT(tex, texCoord0v, lod);
})";
    testCompileNeedsExtensionDirective(
        GL_FRAGMENT_SHADER, kFS, nullptr, "GL_EXT_shader_texture_lod", hasExt,
        hasExt ? "extension is disabled"
               : "'texture2DLodEXT' : no matching overloaded function found",
        hasExt ? "extension is disabled" : "extension is not supported");
}

// GL_EXT_shadow_samplers needs to be enabled to be able to use shadow2DEXT.
TEST_P(GLSLValidationExtensionDirectiveTest_ES3, Sampler2DShadow)
{
    const bool hasExt = IsGLExtensionEnabled("GL_EXT_shadow_samplers");

    constexpr char kFS[] = R"(precision mediump float;
varying vec3 texCoord0v;
uniform mediump sampler2DShadow tex;
void main()
{
    float color = shadow2DEXT(tex, texCoord0v);
})";
    testCompileNeedsExtensionDirective(GL_FRAGMENT_SHADER, kFS, nullptr, "GL_EXT_shadow_samplers",
                                       hasExt, "'sampler2DShadow' : Illegal use of reserved word",
                                       "'sampler2DShadow' : Illegal use of reserved word");
}

// GL_KHR_blend_equation_advanced needs to be enabled to be able to use layout(blend_support_*).
TEST_P(GLSLValidationExtensionDirectiveTest_ES3, AdvancedBlendSupport)
{
    const bool hasExt = IsGLExtensionEnabled("GL_KHR_blend_equation_advanced");
    const bool hasAnyExt =
        hasExt || IsGLExtensionEnabled("GL_KHR_blend_equation_advanced_coherent");

    constexpr char kFS[] = R"(precision highp float;
layout (blend_support_multiply) out;
layout (location = 0) out vec4 oCol;

uniform vec4 uSrcCol;

void main (void)
{
    oCol = uSrcCol;
})";
    testCompileNeedsExtensionDirective(
        GL_FRAGMENT_SHADER, kFS, "#version 300 es", "GL_KHR_blend_equation_advanced", hasExt,
        hasAnyExt ? "extension is disabled" : "extension is not supported",
        hasAnyExt ? "extension is disabled" : "extension is not supported");
}

// GL_OES_sample_variables needs to be enabled to be able to use gl_SampleMask.
TEST_P(GLSLValidationExtensionDirectiveTest_ES3, SampleMask)
{
    const bool hasExt = IsGLExtensionEnabled("GL_OES_sample_variables");

    // This shader is in the deqp test
    // functional_shaders_sample_variables_sample_mask_discard_half_per_sample_default_framebuffer
    constexpr char kFS[] = R"(layout(location = 0) out mediump vec4 fragColor;
void main (void)
{
    for (int i = 0; i < gl_SampleMask.length(); ++i)
            gl_SampleMask[i] = int(0xAAAAAAAA);

    // force per-sample shading
    highp float blue = float(gl_SampleID);

    fragColor = vec4(0.0, 1.0, blue, 1.0);
})";
    testCompileNeedsExtensionDirective(
        GL_FRAGMENT_SHADER, kFS, "#version 300 es", "GL_OES_sample_variables", hasExt,
        hasExt ? "extension is disabled" : "'gl_SampleMask' : undeclared identifier",
        hasExt ? "extension is disabled" : "extension is not supported");
}

// GL_OES_sample_variables needs to be enabled to be able to use gl_SampleMaskIn.
TEST_P(GLSLValidationExtensionDirectiveTest_ES3, SampleMaskIn)
{
    const bool hasExt = IsGLExtensionEnabled("GL_OES_sample_variables");

    // This shader is in the deqp test
    // functional_shaders_sample_variables_sample_mask_in_bit_count_per_sample_multisample_texture_2
    constexpr char kFS[] = R"(layout(location = 0) out mediump vec4 fragColor;
void main (void)
{
    mediump int maskBitCount = 0;
    for (int j = 0; j < gl_SampleMaskIn.length(); ++j)
    {
        for (int i = 0; i < 32; ++i)
        {
            if (((gl_SampleMaskIn[j] >> i) & 0x01) == 0x01)
            {
                ++maskBitCount;
            }
        }
    }

    // force per-sample shading
    highp float blue = float(gl_SampleID);

    if (maskBitCount != 1)
        fragColor = vec4(1.0, 0.0, blue, 1.0);
    else
        fragColor = vec4(0.0, 1.0, blue, 1.0);
})";
    testCompileNeedsExtensionDirective(
        GL_FRAGMENT_SHADER, kFS, "#version 300 es", "GL_OES_sample_variables", hasExt,
        hasExt ? "extension is disabled" : "'gl_SampleMaskIn' : undeclared identifier",
        hasExt ? "extension is disabled" : "extension is not supported");
}

// GL_OES_standard_derivatives needs to be enabled to be able to use dFdx, dFdy and fwidth.
TEST_P(GLSLValidationExtensionDirectiveTest_ES3, StandardDerivatives)
{
    const bool hasExt = IsGLExtensionEnabled("GL_OES_standard_derivatives");

    {
        constexpr char kFS[] = R"(precision mediump float;
varying float x;

void main()
{
    gl_FragColor = vec4(dFdx(x));
})";
        testCompileNeedsExtensionDirective(
            GL_FRAGMENT_SHADER, kFS, nullptr, "GL_OES_standard_derivatives", hasExt,
            hasExt ? "extension is disabled" : "extension is not supported",
            hasExt ? "extension is disabled" : "extension is not supported");
    }

    {
        constexpr char kFS[] = R"(precision mediump float;
varying float x;

void main()
{
    gl_FragColor = vec4(dFdy(x));
})";
        testCompileNeedsExtensionDirective(
            GL_FRAGMENT_SHADER, kFS, nullptr, "GL_OES_standard_derivatives", hasExt,
            hasExt ? "extension is disabled" : "extension is not supported",
            hasExt ? "extension is disabled" : "extension is not supported");
    }

    {
        constexpr char kFS[] = R"(precision mediump float;
varying float x;

void main()
{
    gl_FragColor = vec4(fwidth(x));
})";
        testCompileNeedsExtensionDirective(
            GL_FRAGMENT_SHADER, kFS, nullptr, "GL_OES_standard_derivatives", hasExt,
            hasExt ? "extension is disabled" : "extension is not supported",
            hasExt ? "extension is disabled" : "extension is not supported");
    }
}

// GL_OES_texture_cube_map_array or GL_EXT_texture_cube_map_array needs to be enabled to be able to
// use *samplerCubeArray.
TEST_P(GLSLValidationExtensionDirectiveTest_ES31, TextureCubeMapArray)
{
    const bool hasExt    = IsGLExtensionEnabled("GL_EXT_texture_cube_map_array");
    const bool hasOes    = IsGLExtensionEnabled("GL_OES_texture_cube_map_array");
    const bool hasAnyExt = hasExt || hasOes;

    {
        constexpr char kFS[] = R"(precision mediump float;
uniform highp isamplerCubeArray u_sampler;
void main()
{
    vec4 color = vec4(texture(u_sampler, vec4(0, 0, 0, 0)));
})";

        testCompileNeedsExtensionDirective(
            GL_FRAGMENT_SHADER, kFS, "#version 310 es", "GL_EXT_texture_cube_map_array", hasExt,
            "'isamplerCubeArray' : Illegal use of reserved word",
            hasAnyExt ? "'isamplerCubeArray' : Illegal use of reserved word"
                      : "extension is not supported");
        testCompileNeedsExtensionDirective(
            GL_FRAGMENT_SHADER, kFS, "#version 310 es", "GL_OES_texture_cube_map_array", hasOes,
            "'isamplerCubeArray' : Illegal use of reserved word",
            hasAnyExt ? "'isamplerCubeArray' : Illegal use of reserved word"
                      : "extension is not supported");
    }

    // Make sure support for EXT or OES doesn't imply support for the other.
    if (hasExt && !hasOes)
    {
        constexpr char kFS[] = R"(#version 310 es
#extension GL_OES_texture_cube_map_array: enable
precision mediump float;
uniform highp isamplerCubeArray u_sampler;
void main()
{
    vec4 color = vec4(texture(u_sampler, vec4(0, 0, 0, 0)));
})";
        validateError(GL_FRAGMENT_SHADER, kFS,
                      "'isamplerCubeArray' : Illegal use of reserved word");
    }
    if (!hasExt && hasOes)
    {
        constexpr char kFS[] = R"(#version 310 es
#extension GL_EXT_texture_cube_map_array: enable
precision mediump float;
uniform highp isamplerCubeArray u_sampler;
void main()
{
    vec4 color = vec4(texture(u_sampler, vec4(0, 0, 0, 0)));
})";
        validateError(GL_FRAGMENT_SHADER, kFS,
                      "'isamplerCubeArray' : Illegal use of reserved word");
    }
}

}  // namespace

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(GLSLValidationTest);
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(GLSLValidationTestNoValidation);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLValidationTest_ES3);
ANGLE_INSTANTIATE_TEST_ES3(GLSLValidationTest_ES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLValidationTest_ES31);
ANGLE_INSTANTIATE_TEST_ES31(GLSLValidationTest_ES31);

ANGLE_INSTANTIATE_TEST_ES2(WebGLGLSLValidationTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(WebGL2GLSLValidationTest);
ANGLE_INSTANTIATE_TEST_ES3(WebGL2GLSLValidationTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLValidationClipDistanceTest_ES3);
ANGLE_INSTANTIATE_TEST_ES3_AND(GLSLValidationClipDistanceTest_ES3,
                               ES3_VULKAN().disable(Feature::SupportsAppleClipDistance));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLValidationClipDistanceTest_ES31);
ANGLE_INSTANTIATE_TEST_ES31(GLSLValidationClipDistanceTest_ES31);

ANGLE_INSTANTIATE_TEST_ES2(GLSLValidationTextureRectangleTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLValidationAtomicCounterTest_ES31);
ANGLE_INSTANTIATE_TEST_ES31(GLSLValidationAtomicCounterTest_ES31);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLValidationShaderStorageBlockTest_ES31);
ANGLE_INSTANTIATE_TEST_ES31(GLSLValidationShaderStorageBlockTest_ES31);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLValidationBaseVertexTest_ES3);
ANGLE_INSTANTIATE_TEST(
    GLSLValidationBaseVertexTest_ES3,
    ES3_D3D11().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
    ES3_OPENGL().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
    ES3_OPENGLES().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
    ES3_VULKAN().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
    ES3_VULKAN_SWIFTSHADER().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
    ES3_METAL().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(WebGL2GLSLValidationBaseVertexTest);
ANGLE_INSTANTIATE_TEST_ES3(WebGL2GLSLValidationBaseVertexTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLValidationDrawIDTest);
ANGLE_INSTANTIATE_TEST(
    GLSLValidationDrawIDTest,
    ES3_D3D11().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
    ES3_OPENGL().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
    ES3_OPENGLES().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
    ES3_VULKAN().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
    ES3_VULKAN_SWIFTSHADER().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
    ES3_METAL().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLValidationExtensionDirectiveTest_ES3);
ANGLE_INSTANTIATE_TEST_ES3(GLSLValidationExtensionDirectiveTest_ES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLValidationExtensionDirectiveTest_ES31);
ANGLE_INSTANTIATE_TEST_ES31(GLSLValidationExtensionDirectiveTest_ES31);
