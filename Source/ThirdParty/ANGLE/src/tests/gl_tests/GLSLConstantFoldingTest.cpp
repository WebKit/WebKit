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
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{
class GLSLConstantFoldingTest : public CompilerTest
{
  protected:
    GLSLConstantFoldingTest() {}

    virtual const char *version() { return "300"; }

    // Helper to verify constant folding result.  It's given:
    //
    // * The type of the constant: a varying `v` will be created of this type.
    // * The expression to define and set `c` of that type: The varying is assigned the value of `c`
    //   in the vertex shader.
    // * An expression involving `v` that compares it with the expectation in the fragment shader,
    //   resulting in a bool.
    void test(const char *type, const char *defineC, const char *compareWithV)
    {
        std::stringstream vsSrc;
        vsSrc << "#version " << version() << R"( es
precision highp float;
precision highp int;
flat out )" << type
              << R"( v;
void main()
{
    )" << defineC
              << R"(;
    v = c;

    vec2 pos = vec2(0.);
    switch (gl_VertexID) {
        case 0: pos = vec2(-1., -1.); break;
        case 1: pos = vec2(3., -1.); break;
        case 2: pos = vec2(-1., 3.); break;
    };
    gl_Position = vec4(pos, 0., 1.);
})";

        std::stringstream fsSrc;
        fsSrc << "#version " << version() << R"( es
precision highp float;
precision highp int;
flat in )" << type
              << R"( v;
out vec4 color;
void main()
{
    if ()" << compareWithV
              << R"()
    {
        color = vec4(0, 1, 0, 1);
    }
    else
    {
        color = vec4(1, 0, 0, 1);
    }
})";

        const CompiledShader &vs = compile(GL_VERTEX_SHADER, vsSrc.str().c_str());
        const CompiledShader &fs = compile(GL_FRAGMENT_SHADER, fsSrc.str().c_str());
        ASSERT_TRUE(vs.success());
        ASSERT_TRUE(fs.success());

        const GLuint program = link();
        glUseProgram(program);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    }

    // Check that the expected constant (the result of folding) is present in the translated source
    // for the vertex shader (where constant folding is expected to have happened).
    //
    // Given the various backends and formatting details of the generators, we can't realistically
    // completely verify the translated output.  Often, it's enough to verify that a substring only
    // found if the constant folding is performed is present; the correctness check in test() makes
    // sure constant folding is functionally correct.
    //
    // Additionally, due to the numerous shader massaging in non-GLSL backends, it's easy for
    // expectations to mismatch.  Therefore, these expectations are only checked with the GLSL
    // backend.
    void verifyIsInTranslation(const char *expect)
    {
        if (IsOpenGL())
        {
            EXPECT_TRUE(getCompiledShader(GL_VERTEX_SHADER).verifyInTranslatedSource(expect))
                << expect;
        }
    }
    void verifyIsNotInTranslation(const char *expect)
    {
        if (IsOpenGL())
        {
            EXPECT_TRUE(getCompiledShader(GL_VERTEX_SHADER).verifyNotInTranslatedSource(expect))
                << expect;
        }
    }
};

class GLSLConstantFoldingTest_ES31 : public GLSLConstantFoldingTest
{
  public:
    const char *version() override { return "310"; }
};

// Constant fold integer addition
TEST_P(GLSLConstantFoldingTest, IntegerAdd)
{
    test("int", "const int c = 1124 + 5", "v == 1129");
    verifyIsInTranslation(" 1129");
}

// Constant fold integer subtraction
TEST_P(GLSLConstantFoldingTest, IntegerSub)
{
    test("int", "const int c = 1124 - 5", "v == 1119");
    verifyIsInTranslation(" 1119");
}

// Constant fold integer multiplication
TEST_P(GLSLConstantFoldingTest, IntegerMul)
{
    test("int", "const int c = 1124 * 5", "v == 5620");
    verifyIsInTranslation(" 5620");
}

// Constant fold integer division
TEST_P(GLSLConstantFoldingTest, IntegerDiv)
{
    // Rounding mode of division is undefined in the spec but ANGLE can be expected to round down.
    test("int", "const int c = 1124 / 5", "v == 224");
    verifyIsInTranslation(" 224");
}

// Constant fold integer modulus
TEST_P(GLSLConstantFoldingTest, IntegerMod)
{
    test("int", "const int c = 1124 % 5", "v == 4");
    verifyIsInTranslation(" 4");
}

// Constant fold cross()
TEST_P(GLSLConstantFoldingTest, Cross)
{
    test("vec3", "const vec3 c = cross(vec3(1., 1., 1.), vec3(1., -1., 1.))",
         "all(equal(v, vec3(2., 0., -2.)))");
    verifyIsInTranslation("-2.0");
}

// Constant fold inverse()
TEST_P(GLSLConstantFoldingTest, Inverse2x2)
{
    test("mat2", "const mat2 c = inverse(mat2(2., 3., 5., 7.))",
         "all(equal(v[0], vec2(-7., 3.))) && all(equal(v[1], vec2(5., -2.)))");
    verifyIsInTranslation("-7.0");
}

// Constant fold inverse()
TEST_P(GLSLConstantFoldingTest, Inverse3x3)
{
    test("mat3", "const mat3 c = inverse(mat3(11., 13., 19., 23., 29., 31., 37., 41., 43.))",
         "all(lessThan(abs(v[0] - vec3(31.*41.-29.*43., 13.*43.-19.*41., 19.*29.-13.*31.)/680.), "
         "vec3(0.000001))) && "
         "all(lessThan(abs(v[1] - vec3(23.*43.-31.*37., 19.*37.-11.*43., 11.*31.-19.*23.)/680.), "
         "vec3(0.000001))) && "
         "all(lessThan(abs(v[2] - vec3(29.*37.-23.*41., 11.*41.-13.*37., 13.*23.-11.*29.)/680.), "
         "vec3(0.000001)))");
    verifyIsInTranslation("0.0352");
}

// Constant fold inverse()
TEST_P(GLSLConstantFoldingTest, Inverse4x4)
{
    test("mat4",
         "const mat4 c = inverse(mat4(29., 31., 37., 41., "
         "43., 47., 53., 59., "
         "61., 67., 71., 73., "
         "79., 83., 89., 97.))",
         "all(lessThan(abs(v[0] - vec4(215., -330., -60., 155.)/630.), vec4(0.000001))) && "
         "all(lessThan(abs(v[1] - vec4(-450., 405., 45., -90.)/630.), vec4(0.000001))) && "
         "all(lessThan(abs(v[2] - vec4(425., -330., 129., -76.)/630.), vec4(0.000001))) && "
         "all(lessThan(abs(v[3] - vec4(-180., 225., -108., 27.)/630.), vec4(0.000001)))");
    verifyIsInTranslation("0.3412");
}

// Constant fold determinant()
TEST_P(GLSLConstantFoldingTest, Determinant2x2)
{
    test("float", "const float c = determinant(mat2(2., 3., 5., 7.))", "v == -1.");
    verifyIsInTranslation("-1.0");
}

// Constant fold determinant()
TEST_P(GLSLConstantFoldingTest, Determinant3x3)
{
    test("float", "const float c = determinant(mat3(11., 13., 19., 23., 29., 31., 37., 41., 43.))",
         "v == -680.");
    verifyIsInTranslation("-680.");
}

// Constant fold determinant()
TEST_P(GLSLConstantFoldingTest, Determinant4x4)
{
    test("float",
         "const float c = determinant(mat4(29., 31., 37., 41., "
         "43., 47., 53., 59., "
         "61., 67., 71., 73., "
         "79., 83., 89., 97.))",
         "v == -2520.");
    verifyIsInTranslation("-2520.0");
}

// Constant fold transpose()
TEST_P(GLSLConstantFoldingTest, Transpose3x3)
{
    test("mat3", "const mat3 c = transpose(mat3(11., 13., 19., 23., 29., 31., 37., 41., 43.))",
         "all(lessThan(abs(v[0] - vec3(11., 23., 37.)), vec3(0.000001))) && "
         "all(lessThan(abs(v[1] - vec3(13., 29., 41.)), vec3(0.000001))) && "
         "all(lessThan(abs(v[2] - vec3(19., 31., 43.)), vec3(0.000001)))");
    verifyIsInTranslation("11.0, 23.0");
}

// 0xFFFFFFFF as int should evaluate to -1.
// This is featured in the examples of ESSL3 section 4.1.3. ESSL3 section 12.42
// means that any 32-bit unsigned integer value is a valid literal.
TEST_P(GLSLConstantFoldingTest, ParseWrappedHexIntLiteral)
{
    test("int", "const int c = 0xFFFFFFFF", "v == -1");
    verifyIsInTranslation("-1");
}

// 3000000000 as int should wrap to -1294967296.
// This is featured in the examples of GLSL 4.5, and ESSL behavior should match
// desktop GLSL when it comes to integer parsing.
TEST_P(GLSLConstantFoldingTest, ParseWrappedDecimalIntLiteral)
{
    test("int", "const int c = 3000000000", "v == -1294967296");
    verifyIsInTranslation("-1294967296");
}

// 0xFFFFFFFF as uint should be unchanged.
// This is featured in the examples of ESSL3 section 4.1.3. ESSL3 section 12.42
// means that any 32-bit unsigned integer value is a valid literal.
TEST_P(GLSLConstantFoldingTest, ParseMaxUintLiteral)
{
    test("uint", "const uint c = 0xFFFFFFFFu", "v == 0xFFFFFFFFu");
    verifyIsInTranslation("4294967295");
}

// -1 as uint should wrap to 0xFFFFFFFF.
// This is featured in the examples of ESSL3 section 4.1.3. ESSL3 section 12.42
// means that any 32-bit unsigned integer value is a valid literal.
TEST_P(GLSLConstantFoldingTest, ParseUnaryMinusOneUintLiteral)
{
    test("uint", "const uint c = -1u", "v == 0xFFFFFFFFu");
    verifyIsInTranslation("4294967295");
}

// Constant fold matrix constructor from matrix with identical size
TEST_P(GLSLConstantFoldingTest, ConstructMat2FromMat2)
{
    test("mat2", "const mat2 c = mat2(mat2(0., 1., 2., 3.))",
         "all(equal(v[0], vec2(0., 1.))) && "
         "all(equal(v[1], vec2(2., 3.)))");
}

// Constant fold matrix constructor from scalar
TEST_P(GLSLConstantFoldingTest, ConstructMat2FromScalar)
{
    test("mat2", "const mat2 c = mat2(3)",
         "all(equal(v[0], vec2(3., 0.))) && "
         "all(equal(v[1], vec2(0., 3.)))");
    verifyIsInTranslation("0.0");
}

// Constant fold matrix constructor from vector
TEST_P(GLSLConstantFoldingTest, ConstructMat2FromVector)
{
    test("mat2", "const mat2 c = mat2(vec4(0., 1., 2., 3.))",
         "all(equal(v[0], vec2(0., 1.))) && "
         "all(equal(v[1], vec2(2., 3.)))");
}

// Constant fold matrix constructor from multiple args
TEST_P(GLSLConstantFoldingTest, ConstructMat2FromMultiple)
{
    test("mat2", "const mat2 c = mat2(-1, vec2(0., 1.), vec4(2.))",
         "all(equal(v[0], vec2(-1., 0.))) && "
         "all(equal(v[1], vec2(1., 2.)))");
    verifyIsInTranslation("1.0, 2.0");
}

// Constant fold matrix constructor from larger matrix
TEST_P(GLSLConstantFoldingTest, ConstructMat2FromMat3)
{
    test("mat2", "const mat2 c = mat2(mat3(0., 1., 2., 3., 4., 5., 6., 7., 8.))",
         "all(equal(v[0], vec2(0., 1.))) && "
         "all(equal(v[1], vec2(3., 4.)))");
}

// Constant fold matrix constructor from smaller matrix
TEST_P(GLSLConstantFoldingTest, ConstructMat4x3FromMat3x2)
{
    test("mat4x3", "const mat4x3 c = mat4x3(mat3x2(1., 2., 3., 4., 5., 6.))",
         "all(equal(v[0], vec3(1., 2., 0.))) && "
         "all(equal(v[1], vec3(3., 4., 0.))) && "
         "all(equal(v[2], vec3(5., 6., 1.))) && "
         "all(equal(v[3], vec3(0., 0., 0.)))");
    verifyIsInTranslation("2.0, 0.0");
}

// Constant fold struct comparison when structs are different
TEST_P(GLSLConstantFoldingTest, StructEqualityFalse)
{
    test("int", R"(
struct nested {
    float f;
};
struct S {
    nested a;
    float f;
};
const S s1 = S(nested(0.), 2.);
const S s2 = S(nested(0.), 3.);
const int c = s1 == s2 ? 1 : 0;)",
         "!bool(v)");
}

// Constant fold struct comparison when structs are identical
TEST_P(GLSLConstantFoldingTest, StructEqualityTrue)
{
    test("int", R"(
struct nested {
    float f;
};
struct S {
    nested a;
    float f;
    int i;
};
const S s1 = S(nested(0.), 2., 3);
const S s2 = S(nested(0.), 2., 3);
const int c = s1 == s2 ? 1 : 0;)",
         "bool(v)");
}

// Constant fold indexing of a non-square matrix
TEST_P(GLSLConstantFoldingTest, NonSquareMatrixIndex)
{
    test("vec4", "const vec4 c = mat3x4(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11)[1]",
         "all(equal(v, vec4(4, 5, 6, 7)))");
    verifyIsInTranslation("4.0");
    verifyIsNotInTranslation("[1]");
}

// Constant fold outerProduct with vectors of non-matching length
TEST_P(GLSLConstantFoldingTest, NonSquareOuterProduct)
{
    test("mat3x2", "const mat3x2 c = outerProduct(vec2(2., 3.), vec3(5., 7., 11.))",
         "all(equal(v[0], vec2(10., 15.))) &&"
         "all(equal(v[1], vec2(14., 21.))) &&"
         "all(equal(v[2], vec2(22., 33.)))");
    verifyIsInTranslation("15.0");
}

// Constant fold shift left with different non-matching signedness
TEST_P(GLSLConstantFoldingTest, ShiftLeftMismatchingSignedness)
{
    test("uint", "const uint c = 0xFFFFFFFFu << 31", "v == 0x80000000u");
    verifyIsInTranslation("2147483648");
}

// Constant fold shift right with different non-matching signedness
TEST_P(GLSLConstantFoldingTest, ShiftRightMismatchingSignedness)
{
    test("uint", "const uint c = 0xFFFFFFFFu >> 29", "v == 0x7u");
    verifyIsInTranslation("7");
}

// Constant fold shift right, expecting sign extension
TEST_P(GLSLConstantFoldingTest, ShiftRightSignExtension)
{
    test("int", "const int c = 0x8FFFE000 >> 6", "v == 0xFE3FFF80");
    verifyIsInTranslation("29360256");
}

// Constant fold shift left, such that the number turns from positive to negative
TEST_P(GLSLConstantFoldingTest, ShiftLeftChangeSign)
{
    test("int", "const int c = 0x1FFFFFFF << 3", "v == 0xFFFFFFF8");
    verifyIsInTranslation("-8");
}

// Constant fold divide minimum integer by -1.
// ESSL 3.00.6 section 4.1.3 Integers:
// > However, for the case where the minimum representable value is divided by -1, it is allowed to
// > return either the minimum representable value or the maximum representable value.
//
// ANGLE always returns the maximum value.
TEST_P(GLSLConstantFoldingTest, DivideMinimumIntegerByMinusOne)
{
    test("int", "const int c = 0x80000000 / -1", "v == 0x7FFFFFFF");
    verifyIsInTranslation("2147483647");
}

// Constant fold unsigned addition with overflow
// ESSL 3.00.6 section 4.1.3 Integers:
// > For all precisions, operations resulting in overflow or underflow will not cause any exception,
// > nor will they saturate, rather they will 'wrap' to yield the low-order n bits of the result
// > where n is the size in bits of the integer.
TEST_P(GLSLConstantFoldingTest, UnsignedIntegerAddOverflow)
{
    test("uint", "const uint c = 0xFFFFFFFFu + 43u", "v == 42u");
    verifyIsInTranslation("42");
}

// Constant fold signed addition with overflow
// ESSL 3.00.6 section 4.1.3 Integers:
// > For all precisions, operations resulting in overflow or underflow will not cause any exception,
// > nor will they saturate, rather they will 'wrap' to yield the low-order n bits of the result
// > where n is the size in bits of the integer.
TEST_P(GLSLConstantFoldingTest, SignedIntegerAddOverflow)
{
    test("int", "const int c = 0x7FFFFFFF + 4", "v == -0x7FFFFFFD");
    verifyIsInTranslation("-2147483645");
}

// Constant fold unsigned subtraction with underflow
// ESSL 3.00.6 section 4.1.3 Integers:
// > For all precisions, operations resulting in overflow or underflow will not cause any exception,
// > nor will they saturate, rather they will 'wrap' to yield the low-order n bits of the result
// > where n is the size in bits of the integer.
TEST_P(GLSLConstantFoldingTest, UnsignedIntegerSubUnderflow)
{
    test("uint", "const uint c = 0u - 5u", "v == 0xFFFFFFFBu");
    verifyIsInTranslation("4294967291");
}

// Constant fold signed subtraction with underflow
// ESSL 3.00.6 section 4.1.3 Integers:
// > For all precisions, operations resulting in overflow or underflow will not cause any exception,
// > nor will they saturate, rather they will 'wrap' to yield the low-order n bits of the result
// > where n is the size in bits of the integer.
TEST_P(GLSLConstantFoldingTest, SignedIntegerSubUnderflow)
{
    test("int", "const int c = -0x7FFFFFFF - 7", "v == 0x7FFFFFFA");
    verifyIsInTranslation("2147483642");
}

// Constant fold unsigned multiplication with overflow
// ESSL 3.00.6 section 4.1.3 Integers:
// > For all precisions, operations resulting in overflow or underflow will not cause any exception,
// > nor will they saturate, rather they will 'wrap' to yield the low-order n bits of the result
// > where n is the size in bits of the integer.
TEST_P(GLSLConstantFoldingTest, UnsignedIntegerMulOverflow)
{
    test("uint", "const uint c = 0xFFFFFFFFu * 10u", "v == 0xFFFFFFF6u");
    verifyIsInTranslation("4294967286");
}

// Constant fold signed multiplication with overflow
// ESSL 3.00.6 section 4.1.3 Integers:
// > For all precisions, operations resulting in overflow or underflow will not cause any exception,
// > nor will they saturate, rather they will 'wrap' to yield the low-order n bits of the result
// > where n is the size in bits of the integer.
TEST_P(GLSLConstantFoldingTest, SignedIntegerMulOverflow)
{
    test("int", "const int c = 0x7FFFFFFF * 42", "v == -42");
    verifyIsInTranslation("-42");
}

// Constant fold negation of minimum integer value, which is 0x80000000.
// ESSL 3.00.6 section 4.1.3 Integers:
// > For all precisions, operations resulting in overflow or underflow will not cause any exception,
// > nor will they saturate, rather they will 'wrap' to yield the low-order n bits of the result
// > where n is the size in bits of the integer.
TEST_P(GLSLConstantFoldingTest, SignedIntegerNegateOverflow)
{
    test("int", "const int c = -0x80000000", "v == 0x80000000");
    verifyIsInTranslation("-2147483648");
}

// Constant fold shift right of minimum integer value, which is 0x80000000.
TEST_P(GLSLConstantFoldingTest, SignedIntegerShiftRightMinimumValue)
{
    test("int", "const int c = (0x80000000 >> 1) + (0x80000000 >> 7)", "v == -0x41000000");
    verifyIsInTranslation("-1090519040");
}

// Constant fold shift left by zero.
TEST_P(GLSLConstantFoldingTest, SignedIntegerShiftLeftZero)
{
    test("int", "const int c = 73 << 0", "v == 73");
}

// Constant fold shift right by zero.
TEST_P(GLSLConstantFoldingTest, SignedIntegerShiftRightZero)
{
    test("int", "const int c = 3 >> 0", "v == 3");
}

// Constant fold isinf with an out-of-range value
// ESSL 3.00.6 section 4.1.4 Floats:
// > If the value of the floating point number is too large (small) to be stored as a single
// > precision value, it is converted to positive (negative) infinity.
// ESSL 3.00.6 section 12.4:
// > Mandate support for signed infinities.
TEST_P(GLSLConstantFoldingTest, IsInfTrue)
{
    test("int", "const int c = isinf(1.0e2048) ? 1 : 0", "bool(v)");
}

// Test that floats that are too small to be represented get flushed to zero.
// ESSL 3.00.6 section 4.1.4 Floats:
// > A value with a magnitude too small to be represented as a mantissa and exponent is converted to
// > zero.
TEST_P(GLSLConstantFoldingTest, TooSmallFloat)
{
    test("float", "const float c = 1.0e-2048", "v == 0.");
}

// IEEE 754 dictates that behavior of infinity is derived from limiting cases of real arithmetic.
// lim radians(x) x -> inf = inf
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, RadiansInfinity)
{
    test("float", "const float c = radians(1.0e2048)", "isinf(v) && v > 0.");
}

// IEEE 754 dictates that behavior of infinity is derived from limiting cases of real arithmetic.
// lim degrees(x) x -> inf = inf
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, DegreesInfinity)
{
    test("float", "const float c = degrees(1.0e2048)", "isinf(v) && v > 0.");
}

// IEEE 754 dictates that sinh(inf) = inf.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, SinhInfinity)
{
    test("float", "const float c = sinh(1.0e2048)", "isinf(v) && v > 0.");
}

// IEEE 754 dictates that sinh(-inf) = -inf.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, SinhNegativeInfinity)
{
    test("float", "const float c = sinh(-1.0e2048)", "isinf(v) && v < 0.");
}

// IEEE 754 dictates that cosh(inf) = inf.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, CoshInfinity)
{
    test("float", "const float c = cosh(1.0e2048)", "isinf(v) && v > 0.");
}

// IEEE 754 dictates that cosh(-inf) = inf.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, CoshNegativeInfinity)
{
    test("float", "const float c = cosh(-1.0e2048)", "isinf(v) && v > 0.");
}

// IEEE 754 dictates that asinh(inf) = inf.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, AsinhInfinity)
{
    test("float", "const float c = asinh(1.0e2048)", "isinf(v) && v > 0.");
}

// IEEE 754 dictates that asinh(-inf) = -inf.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, AsinhNegativeInfinity)
{
    test("float", "const float c = asinh(-1.0e2048)", "isinf(v) && v < 0.");
}

// IEEE 754 dictates that acosh(inf) = inf.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, AcoshInfinity)
{
    test("float", "const float c = acosh(1.0e2048)", "isinf(v) && v > 0.");
}

// IEEE 754 dictates that pow or powr(0, inf) = 0.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, PowInfinity)
{
    test("float", "const float c = pow(0.0, 1.0e2048)", "v == 0.");
}

// IEEE 754 dictates that exp(inf) = inf.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, ExpInfinity)
{
    test("float", "const float c = exp(1.0e2048)", "isinf(v) && v > 0.");
}

// IEEE 754 dictates that exp(-inf) = 0.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, ExpNegativeInfinity)
{
    test("float", "const float c = exp(-1.0e2048)", "v == 0.");
}

// IEEE 754 dictates that log(inf) = inf.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, LogInfinity)
{
    test("float", "const float c = log(1.0e2048)", "isinf(v) && v > 0.");
}

// IEEE 754 dictates that exp2(inf) = inf.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, Exp2Infinity)
{
    test("float", "const float c = exp2(1.0e2048)", "isinf(v) && v > 0.");
}

// IEEE 754 dictates that exp2(-inf) = 0.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, Exp2NegativeInfinity)
{
    test("float", "const float c = exp2(-1.0e2048)", "v == 0.");
}

// IEEE 754 dictates that log2(inf) = inf.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, Log2Infinity)
{
    test("float", "const float c = log2(1.0e2048)", "isinf(v) && v > 0.");
}

// IEEE 754 dictates that behavior of infinity is derived from limiting cases of real arithmetic.
// lim sqrt(x) x -> inf = inf
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, SqrtInfinity)
{
    test("float", "const float c = sqrt(1.0e2048)", "isinf(v) && v > 0.");
}

// IEEE 754 dictates that rSqrt(inf) = 0
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, InversesqrtInfinity)
{
    test("float", "const float c = inversesqrt(1.0e2048)", "v == 0.");
}

// IEEE 754 dictates that behavior of infinity is derived from limiting cases of real arithmetic.
// lim length(x) x -> inf = inf
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, LengthInfinity)
{
    test("float", "const float c = length(1.0e2048)", "isinf(v) && v > 0.");
}

// IEEE 754 dictates that behavior of infinity is derived from limiting cases of real arithmetic.
// lim dot(x, y) x -> inf, y > 0 = inf
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, DotInfinity)
{
    test("float", "const float c = dot(1.0e2048, 1.)", "isinf(v) && v > 0.");
}

// IEEE 754 dictates that behavior of infinity is derived from limiting cases of real arithmetic.
// lim dot(vec2(x, y), vec2(z)) x -> inf, finite y, z > 0 = inf
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, DotInfinity2)
{
    test("float", "const float c = dot(vec2(1.0e2048, -1.), vec2(1.))", "isinf(v) && v > 0.");
}

// Faceforward behavior with infinity as a parameter can be derived from dot().
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, FaceForwardInfinity)
{
    test("float", "const float c = faceforward(4., 1.0e2048, 1.)", "v == -4.");
}

// Faceforward behavior with infinity as a parameter can be derived from dot().
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, FaceForwardInfinity2)
{
    test("float", "const float c = faceforward(vec2(4.), vec2(1.0e2048, -1.), vec2(1.)).x",
         "v == -4.");
}

// Test that infinity - finite value evaluates to infinity.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, InfinityMinusFinite)
{
    test("float", "const float c = 1.0e2048 - 1.0e20", "isinf(v) && v > 0.");
}

// Test that -infinity + finite value evaluates to -infinity.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, MinusInfinityPlusFinite)
{
    test("float", "const float c = (-1.0e2048) + 1.0e20", "isinf(v) && v < 0.");
}

// Test that infinity * finite value evaluates to infinity.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, InfinityMultipliedByFinite)
{
    test("float", "const float c = 1.0e2048 * 1.0e-20", "isinf(v) && v > 0.");
}

// Test that infinity * infinity evaluates to infinity.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, InfinityMultipliedByInfinity)
{
    test("float", "const float c = 1.0e2048 * 1.0e2048", "isinf(v) && v > 0.");
}

// Test that infinity * negative infinity evaluates to negative infinity.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, InfinityMultipliedByNegativeInfinity)
{
    test("float", "const float c = 1.0e2048 * (-1.0e2048)", "isinf(v) && v < 0.");
}

// Test that dividing by minus zero results in the appropriately signed infinity.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
// > If both positive and negative zeros are implemented, the correctly signed Inf will be
// > generated.
TEST_P(GLSLConstantFoldingTest, DivideByNegativeZero)
{
    test("float", "const float c = 1. / (-0.)", "isinf(v) && v < 0.");
}

// Test that infinity divided by zero evaluates to infinity.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, InfinityDividedByZero)
{
    test("float", "const float c = 1.0e2048 / 0.", "isinf(v) && v > 0.");
}

// Test that negative infinity divided by zero evaluates to negative infinity.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, MinusInfinityDividedByZero)
{
    test("float", "const float c = (-1.0e2048) / 0.", "isinf(v) && v < 0.");
}

// Test that dividing a finite number by infinity results in zero.
// ESSL 3.00.6 section 4.5.1:
// > Infinities and zeroes are generated as dictated by IEEE.
TEST_P(GLSLConstantFoldingTest, DivideByInfinity)
{
    test("float", "const float c = 1.0e30 / 1.0e2048", "v == 0.");
}

// Test that unsigned bitfieldExtract is folded correctly.
TEST_P(GLSLConstantFoldingTest_ES31, UnsignedBitfieldExtract)
{
    test("uint", "const uint c = bitfieldExtract(0x00110000u, 16, 5)", "v == 0x11u");
    verifyIsInTranslation("17");
}

// Test that unsigned bitfieldExtract to extract 32 bits is folded correctly.
TEST_P(GLSLConstantFoldingTest_ES31, UnsignedBitfieldExtract32Bits)
{
    test("uint", "const uint c = bitfieldExtract(0xFF0000FFu, 0, 32)", "v == 0xFF0000FFu");
}

// Test that signed bitfieldExtract is folded correctly. The higher bits should be set to 1 if the
// most significant bit of the extracted value is 1.
TEST_P(GLSLConstantFoldingTest_ES31, SignedBitfieldExtract)
{
    test("int", "const int c = bitfieldExtract(0x00110000, 16, 5)", "v == -15");
    verifyIsInTranslation("-15");
}

// Test that bitfieldInsert is folded correctly.
TEST_P(GLSLConstantFoldingTest_ES31, BitfieldInsert)
{
    test("uint", "const uint c = bitfieldInsert(0x04501701u, 0x11u, 8, 5)", "v == 0x04501101u");
    verifyIsInTranslation("72356097");
}

// Test that bitfieldInsert to insert 32 bits is folded correctly.
TEST_P(GLSLConstantFoldingTest_ES31, BitfieldInsert32Bits)
{
    test("uint", "const uint c = bitfieldInsert(0xFF0000FFu, 0x11u, 0, 32)", "v == 0x11u");
    verifyIsInTranslation("17");
}

// Test that bitfieldReverse is folded correctly.
TEST_P(GLSLConstantFoldingTest_ES31, BitfieldReverse)
{
    test("uint", "const uint c = bitfieldReverse((1u << 4u) | (1u << 7u))", "v == 0x9000000u");
    verifyIsInTranslation("150994944");
}

// Test that bitCount is folded correctly.
TEST_P(GLSLConstantFoldingTest_ES31, BitCount)
{
    test("int", "const int c = bitCount(0x17103121u)", "v == 10");
    verifyIsInTranslation("10");
}

// Test that findLSB is folded correctly.
TEST_P(GLSLConstantFoldingTest_ES31, FindLSB)
{
    test("int", "const int c = findLSB(0x80010000u)", "v == 16");
    verifyIsInTranslation("16");
}

// Test that findLSB is folded correctly when the operand is zero.
TEST_P(GLSLConstantFoldingTest_ES31, FindLSBZero)
{
    test("int", "const int c = findLSB(0u)", "v == -1");
    verifyIsInTranslation("-1");
}

// Test that findMSB is folded correctly.
TEST_P(GLSLConstantFoldingTest_ES31, FindMSB)
{
    test("int", "const int c = findMSB(0x01000008u)", "v == 24");
    verifyIsInTranslation("24");
}

// Test that findMSB is folded correctly when the operand is zero.
TEST_P(GLSLConstantFoldingTest_ES31, FindMSBZero)
{
    test("int", "const int c = findMSB(0u)", "v == -1");
    verifyIsInTranslation("-1");
}

// Test that findMSB is folded correctly for a negative integer.
// It is supposed to return the index of the most significant bit set to 0.
TEST_P(GLSLConstantFoldingTest_ES31, FindMSBNegativeInt)
{
    test("int", "const int c = findMSB(-8)", "v == 2");
    verifyIsInTranslation("2");
}

// Test that findMSB is folded correctly for -1.
TEST_P(GLSLConstantFoldingTest_ES31, FindMSBMinusOne)
{
    test("int", "const int c = findMSB(-1)", "v == -1");
}

// Test that packUnorm4x8 is folded correctly for a vector of zeroes.
TEST_P(GLSLConstantFoldingTest_ES31, PackUnorm4x8Zero)
{
    test("uint", "const uint c = packUnorm4x8(vec4(0.))", "v == 0u");
}

// Test that packUnorm4x8 is folded correctly for a vector of ones.
TEST_P(GLSLConstantFoldingTest_ES31, PackUnorm4x8One)
{
    test("uint", "const uint c = packUnorm4x8(vec4(1.))", "v == 0xFFFFFFFFu");
    verifyIsInTranslation("4294967295");
}

// Test that packSnorm4x8 is folded correctly for a vector of zeroes.
TEST_P(GLSLConstantFoldingTest_ES31, PackSnorm4x8Zero)
{
    test("uint", "const uint c = packSnorm4x8(vec4(0.))", "v == 0u");
}

// Test that packSnorm4x8 is folded correctly for a vector of ones.
TEST_P(GLSLConstantFoldingTest_ES31, PackSnorm4x8One)
{
    test("uint", "const uint c = packSnorm4x8(vec4(1.))", "v == 0x7F7F7F7Fu");
    verifyIsInTranslation("2139062143");
}

// Test that packSnorm4x8 is folded correctly for a vector of minus ones.
TEST_P(GLSLConstantFoldingTest_ES31, PackSnorm4x8MinusOne)
{
    test("uint", "const uint c = packSnorm4x8(vec4(-1.))", "v == 0x81818181u");
    verifyIsInTranslation("2172748161");
}

// Test that unpackSnorm4x8 is folded correctly when it needs to clamp the result.
TEST_P(GLSLConstantFoldingTest_ES31, UnpackSnorm4x8Clamp)
{
    test("float", "const float c = unpackSnorm4x8(0x00000080u).x", "v == -1.");
    verifyIsInTranslation("-1.0");
}

// Test that unpackUnorm4x8 is folded correctly.
TEST_P(GLSLConstantFoldingTest_ES31, UnpackUnorm4x8)
{
    test("float", "const float c = unpackUnorm4x8(0x007bbeefu).z", "abs(v - 123./255.) < 0.000001");
    verifyIsInTranslation("0.48235");
}

// Test that ldexp is folded correctly.
TEST_P(GLSLConstantFoldingTest_ES31, Ldexp)
{
    test("float", "const float c = ldexp(0.625, 1)", "v == 1.25");
}

// Constant fold ternary
TEST_P(GLSLConstantFoldingTest, Ternary)
{
    test("int", "int c = true ? 1 : v", "v == 1");
    verifyIsNotInTranslation("?");
    verifyIsNotInTranslation("if");
    verifyIsNotInTranslation("else");
}

// Constant fold ternary
TEST_P(GLSLConstantFoldingTest, TernaryInsideExpression)
{
    test("int", "int c = ivec2((true ? 1 : v) + 2, 4).x", "v == 3");
    verifyIsNotInTranslation("?");
    verifyIsNotInTranslation("if");
    verifyIsNotInTranslation("else");
}

// Constant fold indexing of an array
TEST_P(GLSLConstantFoldingTest, ArrayIndex)
{
    test("float", "const float c = float[3](-1., 1., 2.)[2]", "v == 2.");
    verifyIsNotInTranslation("[2]");
}

// Constant fold indexing of an array of array
TEST_P(GLSLConstantFoldingTest_ES31, ArrayOfArrayIndex)
{
    test("float", "const float c = float[2][2](float[2](-1., 1.), float[2](2., 3.))[1][0]",
         "v == 2.");
    verifyIsNotInTranslation("[1]");
    verifyIsNotInTranslation("[0]");
}

// Constant fold indexing of an array stashed in another variable
TEST_P(GLSLConstantFoldingTest, NamedArrayIndex)
{
    test("float", R"(const float[3] arr = float[3](-1., 1., 2.);
const float c = arr[1])",
         "v == 1.");
    verifyIsNotInTranslation("[1]");
}

// Constant fold indexing of an array of array stashed in another variable
TEST_P(GLSLConstantFoldingTest_ES31, NamedArrayOfArrayIndex)
{
    test("float[2]", R"(const float[2][2] arr = float[2][2](float[2](-1., 1.), float[2](2., 3.));
const float[2] c = arr[1])",
         "v[0] == 2. && v[1] == 3.");
}

// Constant fold indexing of an array of mixed constant and non-constant values (without side
// effect).
TEST_P(GLSLConstantFoldingTest, ArrayMixedArgumentsIndex)
{
    test("float", R"(float c = float[2](v, 1.)[1])", "v == 1.");
    verifyIsNotInTranslation("[1]");
}

// Constant fold indexing of an array of mixed constant and non-constant values with side
// effect should not discard the side effect.
TEST_P(GLSLConstantFoldingTest, ArrayMixedArgumentsWithSideEffectIndex)
{
    test("float", R"(float sideEffectTarget = 0.;
float indexedElement = float[3](sideEffectTarget = 5., 11., 102.)[1];
float c = indexedElement + sideEffectTarget)",
         "v == 16.");
}

// Constant fold equality of constructed arrays.
TEST_P(GLSLConstantFoldingTest, ArrayEqualityFalse)
{
    test("float", "const float c = float[3](2., 1., 1.) == float[3](2., 1., -1.) ? 1. : 2.",
         "v == 2.");
}

// Constant fold equality of constructed arrays.
TEST_P(GLSLConstantFoldingTest, ArrayEqualityTrue)
{
    test("float", "const float c = float[3](2., 1., -1.) == float[3](2., 1., -1.) ? 1. : 2.",
         "v == 1.");
}

// Constant fold equality of constructed arrays stashed in variables.
TEST_P(GLSLConstantFoldingTest, NamedArrayEqualityFalse)
{
    test("float", R"(const float[3] arrA = float[3](-1., 1., 2.);
const float[3] arrB = float[3](1., 1., 2.);
float c = arrA == arrB ? 1. : 2.)",
         "v == 2.");
}

// Constant fold equality of constructed arrays stashed in variables.
TEST_P(GLSLConstantFoldingTest, NamedArrayEqualityTrue)
{
    test("float", R"(const float[3] arrA = float[3](-1., 1., 2.);
const float[3] arrB = float[3](-1., 1., 2.);
float c = arrA == arrB ? 1. : 2.)",
         "v == 1.");
}

// Constant fold equality of constructed arrays of arrays.
TEST_P(GLSLConstantFoldingTest_ES31, ArrayOfArrayEqualityFalse)
{
    test("float", R"(const float c = float[2][2](float[2](-1., 1.), float[2](2., 3.))
                                  == float[2][2](float[2](-1., 1.), float[2](2., 1000.)) ? 1. : 2.)",
         "v == 2.");
}

// Constant fold equality of constructed arrays.
TEST_P(GLSLConstantFoldingTest_ES31, ArrayOfArrayEqualityTrue)
{
    test("float", R"(const float c = float[2][2](float[2](-1., 1.), float[2](2., 3.))
                                  == float[2][2](float[2](-1., 1.), float[2](2., 3.)) ? 1. : 2.)",
         "v == 1.");
}

// Constant fold casting a negative float to uint.
// ESSL 3.00.6 section 5.4.1 specifies this as an undefined conversion.
TEST_P(GLSLConstantFoldingTest, NegativeFloatToUint)
{
    test("uint", "const uint c = uint(-1.)", "v == 0xFFFFFFFFu");
    verifyIsInTranslation("4294967295");
}

// Constant fold casting a negative float to uint inside a uvec constructor.
// ESSL 3.00.6 section 5.4.1 specifies this as an undefined conversion.
TEST_P(GLSLConstantFoldingTest, NegativeFloatToUvec)
{
    test("uint", "const uint c = uvec4(2., 1., vec2(0., -1.)).w", "v == 0xFFFFFFFFu");
    verifyIsInTranslation("4294967295");
}

// Constant fold casting a negative float to uint inside a uvec constructor, but that which is not
// used by the constructor.
TEST_P(GLSLConstantFoldingTest, NegativeFloatInsideUvecConstructorButOutOfRange)
{
    test("uint", "const uint c = uvec2(1., vec2(0., -1.)).x", "v == 1u");
}

// Constant fold a large float (above max signed int) to uint.
TEST_P(GLSLConstantFoldingTest, LargeFloatToUint)
{
    test("uint", "const uint c = uint(3221225472.)", "v == 3221225472u");
}

// Constant fold modulus with a negative dividend.
TEST_P(GLSLConstantFoldingTest, IntegerModulusNegativeDividend)
{
    test("int", "const int c = (-5) % 3", "v == 0");
}

// Constant fold modulus with a negative divisor.
TEST_P(GLSLConstantFoldingTest, IntegerModulusNegativeDivisor)
{
    test("int", "const int c = 5 % (-3)", "v == 0");
}

// Constant fold isnan with multiple components
TEST_P(GLSLConstantFoldingTest_ES31, IsnanMultipleComponents)
{
    test("ivec4", "const ivec4 c = ivec4(mix(ivec2(2), ivec2(3), isnan(vec2(1., 0. / 0.))), 4, 5)",
         "all(equal(v, ivec4(2, 3, 4, 5)))");
}

// Constant fold isinf with multiple components
TEST_P(GLSLConstantFoldingTest_ES31, IsinfMultipleComponents)
{
    test("ivec4",
         "const ivec4 c = ivec4(mix(ivec2(2), ivec2(3), isinf(vec2(0.0, 1.0e2048))), 4, 5)",
         "all(equal(v, ivec4(2, 3, 4, 5)))");
}

// Constant fold floatBitsToInt with multiple components
TEST_P(GLSLConstantFoldingTest, FloatBitsToIntMultipleComponents)
{
    test("ivec4", "const ivec4 c = ivec4(floatBitsToInt(vec2(0.0, 1.0)), 4, 5)",
         "all(equal(v, ivec4(0, 0x3f800000, 4, 5)))");
}

// Constant fold floatBitsToUint with multiple components
TEST_P(GLSLConstantFoldingTest, FloatBitsToUintMultipleComponents)
{
    test("ivec4", "const ivec4 c = ivec4(floatBitsToUint(vec2(0.0, 1.0)), 4, 5)",
         "all(equal(v, ivec4(0, 0x3f800000, 4, 5)))");
}

// Constant fold intBitsToFloat with multiple components
TEST_P(GLSLConstantFoldingTest, IntBitsToFloatMultipleComponents)
{
    test("vec4", "const vec4 c = vec4(intBitsToFloat(ivec2(0, 0x3f800000)), 0.25, 0.5)",
         "all(equal(v, vec4(0., 1., 0.25, 0.5)))");
}

// Constant fold uintBitsToFloat with multiple components
TEST_P(GLSLConstantFoldingTest, UintBitsToFloatMultipleComponents)
{
    test("vec4", "const vec4 c = vec4(uintBitsToFloat(uvec2(0U, 0x3f800000U)), 0.25, 0.5)",
         "all(equal(v, vec4(0., 1., 0.25, 0.5)))");
}

// Test that infinity - infinity evaluates to NaN.
TEST_P(GLSLConstantFoldingTest, InfinityMinusInfinity)
{
    test("float", "const float c = 1.0e2048 - 1.0e2048", "isnan(v)");
}

// Test that infinity + negative infinity evaluates to NaN.
TEST_P(GLSLConstantFoldingTest, InfinityPlusNegativeInfinity)
{
    test("float", "const float c = 1.0e2048 + (-1.0e2048)", "isnan(v)");
}

// Test that infinity multiplied by zero evaluates to NaN.
TEST_P(GLSLConstantFoldingTest, InfinityMultipliedByZero)
{
    test("float", "const float c = 1.0e2048 * 0.", "isnan(v)");
}

// Test that infinity divided by infinity evaluates to NaN.
TEST_P(GLSLConstantFoldingTest, InfinityDividedByInfinity)
{
    test("float", "const float c = 1.0e2048 / 1.0e2048", "isnan(v)");
}

// Test that zero divided by zero evaluates to NaN.
TEST_P(GLSLConstantFoldingTest, ZeroDividedByZero)
{
    test("float", "const float c = 0. / 0.", "isnan(v)");
}

// Test that addition that overflows is evaluated correctly.
TEST_P(GLSLConstantFoldingTest, FloatOverflowAdd)
{
    test("float", "const float c = 2.0e38 + 2.0e38", "isinf(v) && v > 0.");
}

// Test that subtraction that overflows is evaluated correctly.
TEST_P(GLSLConstantFoldingTest, FloatOverflowSubtract)
{
    test("float", "const float c = 2.0e38 - (-2.0e38)", "isinf(v) && v > 0.");
}

// Test that multiplication that overflows is evaluated correctly.
TEST_P(GLSLConstantFoldingTest, FloatOverflowMultiply)
{
    test("float", "const float c = 1.0e30 * 1.0e10", "isinf(v) && v > 0.");
}

// Test that division that overflows is evaluated correctly.
TEST_P(GLSLConstantFoldingTest, FloatOverflowDivide)
{
    test("float", "const float c = 1.0e30 / 1.0e-10", "isinf(v) && v > 0.");
}
}  // namespace

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLConstantFoldingTest);
ANGLE_INSTANTIATE_TEST_ES3(GLSLConstantFoldingTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLConstantFoldingTest_ES31);
ANGLE_INSTANTIATE_TEST_ES31(GLSLConstantFoldingTest_ES31);
