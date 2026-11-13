//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GLSLPrecedenceTest: Tests for GLSL operator precedence and associativity with uniforms.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{
class GLSLPrecedenceTest : public ANGLETest<>
{
  protected:
    GLSLPrecedenceTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    static constexpr float kA = 10.0f;
    static constexpr float kB = 4.0f;
    static constexpr float kC = 2.0f;
    static constexpr float kD = 1.0f;

    static constexpr int kIA = 3;
    static constexpr int kIB = 5;
    static constexpr int kIC = 2;

    static constexpr float kDividend = 16.0f;

    static constexpr float kVA0 = 1.0f;
    static constexpr float kVA1 = 2.0f;
    static constexpr float kVB0 = 3.0f;
    static constexpr float kVB1 = 4.0f;

    virtual void runTest(const std::string &conditionStr, const char *exprStr)
    {
        std::stringstream fs;
        fs << R"(precision mediump float;
uniform float a, b, c, d;
uniform int ia, ib, ic;
uniform vec2 va, vb;
void main()
{
    bool ok = )"
           << conditionStr << R"(;
    gl_FragColor = ok ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
}
)";

        ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), fs.str().c_str());
        glUseProgram(program);
        glUniform1f(glGetUniformLocation(program, "a"), kA);
        glUniform1f(glGetUniformLocation(program, "b"), kB);
        glUniform1f(glGetUniformLocation(program, "c"), kC);
        glUniform1f(glGetUniformLocation(program, "d"), kD);
        glUniform1i(glGetUniformLocation(program, "ia"), kIA);
        glUniform1i(glGetUniformLocation(program, "ib"), kIB);
        glUniform1i(glGetUniformLocation(program, "ic"), kIC);
        glUniform2f(glGetUniformLocation(program, "va"), kVA0, kVA1);
        glUniform2f(glGetUniformLocation(program, "vb"), kVB0, kVB1);

        drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green) << "Failed expression: " << exprStr;
    }

    void testFloat(const char *expr, float expected)
    {
        std::stringstream cond;
        cond << "abs((" << expr << ") - float(" << expected << ")) < 0.01";
        runTest(cond.str(), expr);
    }

    void testInt(const char *expr, int expected)
    {
        std::stringstream cond;
        cond << "(" << expr << ") == (" << expected << ")";
        runTest(cond.str(), expr);
    }

    void testBool(const char *expr, bool expected)
    {
        std::stringstream cond;
        cond << "(" << expr << ") == " << (expected ? "true" : "false");
        runTest(cond.str(), expr);
    }

    void testVec2(const char *expr, float ex, float ey)
    {
        std::stringstream cond;
        cond << "distance((" << expr << "), vec2(" << ex << ", " << ey << ")) < 0.01";
        runTest(cond.str(), expr);
    }
};

class GLSLPrecedenceTest_ES3 : public GLSLPrecedenceTest
{
  protected:
    static constexpr int kID = 4;

    static constexpr uint32_t kUA = 13u;
    static constexpr uint32_t kUB = 11u;
    static constexpr uint32_t kUC = 6u;
    static constexpr uint32_t kUD = 3u;

    static constexpr uint32_t kSA = 2u;
    static constexpr uint32_t kSB = 1u;

    static constexpr int kMA = 17;
    static constexpr int kMB = 10;
    static constexpr int kMC = 4;
    static constexpr int kMD = 2;

    void runTest(const std::string &conditionStr, const char *exprStr) override
    {
        std::stringstream fs;
        fs << R"(#version 300 es
precision highp float;
precision highp int;
uniform float a, b, c, d;
uniform int ia, ib, ic, id;
uniform uint ua, ub, uc, ud;
uniform uint sa, sb;
uniform int ma, mb, mc, md;
uniform vec2 va, vb;
out vec4 fragColor;
void main()
{
    bool ok = )"
           << conditionStr << R"(;
    fragColor = ok ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
}
)";

        ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), fs.str().c_str());
        glUseProgram(program);
        glUniform1f(glGetUniformLocation(program, "a"), kA);
        glUniform1f(glGetUniformLocation(program, "b"), kB);
        glUniform1f(glGetUniformLocation(program, "c"), kC);
        glUniform1f(glGetUniformLocation(program, "d"), kD);
        glUniform1i(glGetUniformLocation(program, "ia"), kIA);
        glUniform1i(glGetUniformLocation(program, "ib"), kIB);
        glUniform1i(glGetUniformLocation(program, "ic"), kIC);
        glUniform1i(glGetUniformLocation(program, "id"), kID);
        glUniform1ui(glGetUniformLocation(program, "ua"), kUA);
        glUniform1ui(glGetUniformLocation(program, "ub"), kUB);
        glUniform1ui(glGetUniformLocation(program, "uc"), kUC);
        glUniform1ui(glGetUniformLocation(program, "ud"), kUD);
        glUniform1ui(glGetUniformLocation(program, "sa"), kSA);
        glUniform1ui(glGetUniformLocation(program, "sb"), kSB);
        glUniform1i(glGetUniformLocation(program, "ma"), kMA);
        glUniform1i(glGetUniformLocation(program, "mb"), kMB);
        glUniform1i(glGetUniformLocation(program, "mc"), kMC);
        glUniform1i(glGetUniformLocation(program, "md"), kMD);
        glUniform2f(glGetUniformLocation(program, "va"), kVA0, kVA1);
        glUniform2f(glGetUniformLocation(program, "vb"), kVB0, kVB1);

        drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green) << "Failed expression: " << exprStr;
    }

    void testUInt(const char *expr, uint32_t expected)
    {
        std::stringstream cond;
        cond << "(" << expr << ") == (" << expected << "u)";
        runTest(cond.str(), expr);
    }
};

// ============================================================================
// ESSL 1.00 Tests
// ============================================================================

// 1. Additive & Multiplicative, unary negation and postfix swizzle:
// Verify AddMul precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, AddMul)
{
    testFloat("a + b * c", kA + kB * kC);
}

// Verify ParenAddMul precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, ParenAddMul)
{
    testFloat("(a + b) * c", (kA + kB) * kC);
}

// Verify MulParenSub precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, MulParenSub)
{
    testFloat("c * (a - b)", kC * (kA - kB));
}

// Verify DivParenSubAdd precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, DivParenSubAdd)
{
    testFloat("(a - b) / (c + d)", (kA - kB) / (kC + kD));
}

// Verify UnaryDoubleNeg precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, UnaryDoubleNeg)
{
    testFloat("-(-a)", kA);
}

// Verify UnaryNegParenSub precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, UnaryNegParenSub)
{
    testFloat("-(a - b)", -(kA - kB));
}

// Verify VectorSwizzleMulAdd precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, VectorSwizzleMulAdd)
{
    testVec2("(va + vb * c).yx", kVA1 + kVB1 * kC, kVA0 + kVB0 * kC);
}

// 2. Left-associativity vs. right-nested parenthesization:
// Verify SubSubAssociativity precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, SubSubAssociativity)
{
    testFloat("a - b - c", kA - kB - kC);
}

// Verify SubParenSub precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, SubParenSub)
{
    testFloat("a - (b - c)", kA - (kB - kC));
}

// Verify SubParenAdd precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, SubParenAdd)
{
    testFloat("a - (b + c)", kA - (kB + kC));
}

// Verify DivDivAssociativity precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, DivDivAssociativity)
{
    testFloat("16.0 / b / c", kDividend / kB / kC);
}

// Verify DivParenDiv precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, DivParenDiv)
{
    testFloat("16.0 / (b / c)", kDividend / (kB / kC));
}

// Verify DivParenMul precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, DivParenMul)
{
    testFloat("16.0 / (b * c)", kDividend / (kB * kC));
}

// Verify IntMulDivAssociativity precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, IntMulDivAssociativity)
{
    testInt("ia * ib / ic", kIA * kIB / kIC);
}

// Verify IntMulParenDiv precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, IntMulParenDiv)
{
    testInt("ia * (ib / ic)", kIA * (kIB / kIC));
}

// 3. Relational, equality and logical xor:
// Verify RelationalEqual precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, RelationalEqual)
{
    testBool("(a > b) == (b > c)", (kA > kB) == (kB > kC));
}

// Verify RelationalNotEqual precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, RelationalNotEqual)
{
    testBool("(a < b) != (b >= c)", (kA < kB) != (kB >= kC));
}

// Verify RelationalXor precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, RelationalXor)
{
    testBool("(a > b) ^^ (b < c)", (kA > kB) != (kB < kC));
}

// Verify ChainedRelationalEqual precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, ChainedRelationalEqual)
{
    testBool("(a > b) == (b < c) == (c > b)", ((kA > kB) == (kB < kC)) == (kC > kB));
}

// Verify ChainedRelationalXor precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, ChainedRelationalXor)
{
    testBool("(a > b) ^^ (b > c) ^^ (c > d)", ((kA > kB) != (kB > kC)) != (kC > kD));
}

// Verify NotEqualAndNotLess precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, NotEqualAndNotLess)
{
    testBool("!(a == b) && !(a < b)", !(kA == kB) && !(kA < kB));
}

// Verify MixedArithmeticRelationalLogical precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest, MixedArithmeticRelationalLogical)
{
    testBool("(a - b > c * d) && (a + b == c * (b + c) + c)",
             (kA - kB > kC * kD) && (kA + kB == kC * (kB + kC) + kC));
}

// ============================================================================
// ESSL 3.00 Tests
// ============================================================================

// 1. Additive & Multiplicative, unary negation and postfix swizzle:
// Verify AddMul precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, AddMul)
{
    testFloat("a + b * c", kA + kB * kC);
}

// Verify ParenAddMul precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ParenAddMul)
{
    testFloat("(a + b) * c", (kA + kB) * kC);
}

// Verify MulAdd precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, MulAdd)
{
    testFloat("a * b + c", kA * kB + kC);
}

// Verify MulParenAdd precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, MulParenAdd)
{
    testFloat("a * (b + c)", kA * (kB + kC));
}

// Verify MulParenSub precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, MulParenSub)
{
    testFloat("c * (a - b)", kC * (kA - kB));
}

// Verify DivParenSubAdd precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, DivParenSubAdd)
{
    testFloat("(a - b) / (c + d)", (kA - kB) / (kC + kD));
}

// Verify UnaryDoubleNeg precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, UnaryDoubleNeg)
{
    testFloat("-(-a)", kA);
}

// Verify UnaryNegParenSub precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, UnaryNegParenSub)
{
    testFloat("-(a - b)", -(kA - kB));
}

// Verify VectorSwizzleMulAdd precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, VectorSwizzleMulAdd)
{
    testVec2("(va + vb * c).yx", kVA1 + kVB1 * kC, kVA0 + kVB0 * kC);
}

// 2. Left-associativity vs. right-nested parenthesization:
// Verify SubSubAssociativity precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, SubSubAssociativity)
{
    testFloat("a - b - c", kA - kB - kC);
}

// Verify SubParenSub precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, SubParenSub)
{
    testFloat("a - (b - c)", kA - (kB - kC));
}

// Verify SubParenAdd precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, SubParenAdd)
{
    testFloat("a - (b + c)", kA - (kB + kC));
}

// Verify DivDivAssociativity precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, DivDivAssociativity)
{
    testFloat("16.0 / b / c", kDividend / kB / kC);
}

// Verify DivParenDiv precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, DivParenDiv)
{
    testFloat("16.0 / (b / c)", kDividend / (kB / kC));
}

// Verify DivParenMul precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, DivParenMul)
{
    testFloat("16.0 / (b * c)", kDividend / (kB * kC));
}

// Verify IntMulDivAssociativity precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, IntMulDivAssociativity)
{
    testInt("ia * ib / ic", kIA * kIB / kIC);
}

// Verify IntMulParenDiv precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, IntMulParenDiv)
{
    testInt("ia * (ib / ic)", kIA * (kIB / kIC));
}

// 3. Relational, equality and logical xor (non-associative in WGSL):
// Verify RelationalEqual precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, RelationalEqual)
{
    testBool("(a > b) == (b > c)", (kA > kB) == (kB > kC));
}

// Verify RelationalNotEqual precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, RelationalNotEqual)
{
    testBool("(a < b) != (b >= c)", (kA < kB) != (kB >= kC));
}

// Verify RelationalXor precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, RelationalXor)
{
    testBool("(a > b) ^^ (b < c)", (kA > kB) != (kB < kC));
}

// Verify ChainedRelationalEqual precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ChainedRelationalEqual)
{
    testBool("(a > b) == (b < c) == (c > b)", ((kA > kB) == (kB < kC)) == (kC > kB));
}

// Verify ChainedRelationalXor precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ChainedRelationalXor)
{
    testBool("(a > b) ^^ (b > c) ^^ (c > d)", ((kA > kB) != (kB > kC)) != (kC > kD));
}

// Verify NotEqualAndNotLess precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, NotEqualAndNotLess)
{
    testBool("!(a == b) && !(a < b)", !(kA == kB) && !(kA < kB));
}

// Verify MixedArithmeticRelationalLogical precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, MixedArithmeticRelationalLogical)
{
    testBool("(a - b > c * d) && (a + b == c * (b + c) + c)",
             (kA - kB > kC * kD) && (kA + kB == kC * (kB + kC) + kC));
}

// 4. Shift operators (non-associative in WGSL, and cannot mix with + or * without parens):
// Verify ShiftAddPrecedence precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ShiftAddPrecedence)
{
    testInt("ia << sb + sb", kIA << (kSB + kSB));
}

// Verify ShiftParenAdd precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ShiftParenAdd)
{
    testInt("ia << (sa + sb)", kIA << (kSA + kSB));
}

// Verify ParenAddShift precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ParenAddShift)
{
    testInt("(ia + id) << sb", (kIA + kID) << kSB);
}

// Verify ParenShiftAdd precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ParenShiftAdd)
{
    testInt("(ia << sa) + id", (kIA << kSA) + kID);
}

// Verify AddParenShift precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, AddParenShift)
{
    testInt("id + (ia << sb)", kID + (kIA << kSB));
}

// Verify SubParenShift precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, SubParenShift)
{
    testInt("id - (ia >> sb)", kID - (kIA >> kSB));
}

// Verify MulShiftPrecedence precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, MulShiftPrecedence)
{
    testInt("ia * id >> sb", (kIA * kID) >> kSB);
}

// Verify ParenShiftMul precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ParenShiftMul)
{
    testInt("(ia << sb) * id", (kIA << kSB) * kID);
}

// Verify MulParenShift precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, MulParenShift)
{
    testInt("ia * (id >> sb)", kIA * (kID >> kSB));
}

// Verify ShiftShiftAssociativity precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ShiftShiftAssociativity)
{
    testInt("ia << sa << sb", (kIA << kSA) << kSB);
}

// Verify ShiftParenShift precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ShiftParenShift)
{
    testInt("ia << (sa << sb)", kIA << (kSA << kSB));
}

// Verify ChainedShift precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ChainedShift)
{
    testInt("(id << sa) >> sb >> sb", ((kID << kSA) >> kSB) >> kSB);
}

// Verify ShiftRelationalMixed precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ShiftRelationalMixed)
{
    testBool("(ia + id < id << sa) && (ia << sb == id + int(sa))",
             (kIA + kID < (kID << kSA)) && ((kIA << kSB) == (kID + (int)kSA)));
}

// 5. Bitwise operators (&, ^, |, ~):
// Verify BitwiseAndAssociativity precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, BitwiseAndAssociativity)
{
    testUInt("ua & ub & ud", (kUA & kUB) & kUD);
}

// Verify BitwiseXorAssociativity precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, BitwiseXorAssociativity)
{
    testUInt("ua ^ ub ^ ud", (kUA ^ kUB) ^ kUD);
}

// Verify BitwiseOrAssociativity precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, BitwiseOrAssociativity)
{
    testUInt("ua | ub | uc", (kUA | kUB) | kUC);
}

// Verify BitwiseAndOrPrecedence precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, BitwiseAndOrPrecedence)
{
    testUInt("ua & uc | ud", (kUA & kUC) | kUD);
}

// Verify BitwiseAndParenOr precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, BitwiseAndParenOr)
{
    testUInt("ua & (uc | ud)", kUA & (kUC | kUD));
}

// Verify BitwiseOrAndPrecedence precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, BitwiseOrAndPrecedence)
{
    testUInt("ud | ua & uc", kUD | (kUA & kUC));
}

// Verify BitwiseParenOrAnd precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, BitwiseParenOrAnd)
{
    testUInt("(ud | ua) & uc", (kUD | kUA) & kUC);
}

// Verify BitwiseAndXorPrecedence precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, BitwiseAndXorPrecedence)
{
    testUInt("ua & ub ^ uc", (kUA & kUB) ^ kUC);
}

// Verify BitwiseAndParenXor precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, BitwiseAndParenXor)
{
    testUInt("ua & (ub ^ uc)", kUA & (kUB ^ kUC));
}

// Verify BitwiseXorOrPrecedence precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, BitwiseXorOrPrecedence)
{
    testUInt("ua ^ uc | ub", (kUA ^ kUC) | kUB);
}

// Verify BitwiseXorParenOr precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, BitwiseXorParenOr)
{
    testUInt("ua ^ (uc | ub)", kUA ^ (kUC | kUB));
}

// Verify AddBitwiseAndPrecedence precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, AddBitwiseAndPrecedence)
{
    testUInt("uc + ud & ua", (kUC + kUD) & kUA);
}

// Verify AddParenBitwiseAnd precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, AddParenBitwiseAnd)
{
    testUInt("uc + (ud & ua)", kUC + (kUD & kUA));
}

// Verify ParenBitwiseAndAdd precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ParenBitwiseAndAdd)
{
    testUInt("(uc & ud) + ua", (kUC & kUD) + kUA);
}

// Verify MulBitwiseXorPrecedence precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, MulBitwiseXorPrecedence)
{
    testUInt("uc * ud ^ ua", (kUC * kUD) ^ kUA);
}

// Verify ParenBitwiseXorMul precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ParenBitwiseXorMul)
{
    testUInt("(uc ^ ud) * uint(ia)", (kUC ^ kUD) * (uint32_t)kIA);
}

// Verify ShiftBitwiseAndPrecedence precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ShiftBitwiseAndPrecedence)
{
    testUInt("ud << sb & uc", (kUD << kSB) & kUC);
}

// Verify ParenBitwiseAndShift precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ParenBitwiseAndShift)
{
    testUInt("(uc & ud) << sb", (kUC & kUD) << kSB);
}

// Verify BitwiseNotAndPrecedence precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, BitwiseNotAndPrecedence)
{
    testUInt("~ua & ub", (~kUA) & kUB);
}

// Verify BitwiseNotParenAnd precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, BitwiseNotParenAnd)
{
    testUInt("~(ua & ub)", ~(kUA & kUB));
}

// Verify BitwiseRelationalMixed precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, BitwiseRelationalMixed)
{
    testBool("((ua & ub) == uc + ud) && ((uc | ud) != (ua ^ ub))",
             ((kUA & kUB) == kUC + kUD) && ((kUC | kUD) != (kUA ^ kUB)));
}

// 6. Integer modulo:
// Verify ModuloModuloAssociativity precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ModuloModuloAssociativity)
{
    testInt("ma % mb % mc", (kMA % kMB) % kMC);
}

// Verify ModuloParenModulo precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ModuloParenModulo)
{
    testInt("ma % (mb % mc)", kMA % (kMB % kMC));
}

// Verify ModuloMulAssociativity precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ModuloMulAssociativity)
{
    testInt("ma % mb * mc", (kMA % kMB) * kMC);
}

// Verify ModuloParenMul precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ModuloParenMul)
{
    testInt("ma % (mc * md)", kMA % (kMC * kMD));
}

// Verify MulParenModulo precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, MulParenModulo)
{
    testInt("mc * (ma % mb)", kMC * (kMA % kMB));
}

// Verify AddModuloPrecedence precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, AddModuloPrecedence)
{
    testInt("ma + mb % mc", kMA + (kMB % kMC));
}

// Verify ParenAddModulo precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ParenAddModulo)
{
    testInt("(ma + mb) % mc", (kMA + kMB) % kMC);
}

// Verify ModuloAddPrecedence precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, ModuloAddPrecedence)
{
    testInt("ia % ib + ic", kIA % kIB + kIC);
}

// Verify AddParenModulo precedence and associativity behavior.
TEST_P(GLSLPrecedenceTest_ES3, AddParenModulo)
{
    testInt("ia + (ib % ic)", kIA + (kIB % kIC));
}

}  // anonymous namespace

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(GLSLPrecedenceTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLPrecedenceTest_ES3);
ANGLE_INSTANTIATE_TEST_ES3_AND(GLSLPrecedenceTest_ES3, ES3_WEBGPU());
