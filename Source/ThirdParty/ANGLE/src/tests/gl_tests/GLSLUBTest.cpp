//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

#include "test_utils/angle_test_configs.h"
#include "test_utils/gl_raii.h"
#include "util/shader_utils.h"

namespace
{
using namespace angle;

class GLSLUBTest : public ANGLETest<>
{
  protected:
    GLSLUBTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// grep TEST_P src/tests/gl_tests/GLSLUBTest.cpp
// For prefixes Add, Sub find:
//   IntInt
//   IntIvec
//   IvecInt
//   IvecIvec
//   AssignIvecInt
//   AssignIvecIvec

// Test int + int with overflow. Expect wraparound.
TEST_P(GLSLUBTest, AddIntIntOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    int r0 = u + -1;
    int r1 = u + 0;
    int r2 = 2147483646 + u;
    int r3 = u + 2147483647;

    gl_FragColor.r = r0 == 1 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == 2 ? 1.0 : 0.0;
    gl_FragColor.b = r2 == -2147483648 ? 1.0 : 0.0;
    gl_FragColor.a = r3 == -2147483647 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 2);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int + ivec overflow. Expect wraparound.
TEST_P(GLSLUBTest, AddIntIvecOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    ivec4 r = u + ivec4(0, -1, 1, 2147483647);
    gl_FragColor.r = r.x == 2 ? 1.0 : 0.0;
    gl_FragColor.g = r.y == 1 ? 1.0 : 0.0;
    gl_FragColor.b = r.z == 3 ? 1.0 : 0.0;
    gl_FragColor.a = r.w == -2147483647 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 2);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int + ivec overflow. Expect wraparound.
TEST_P(GLSLUBTest, AddIvecIntOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    ivec4 r = ivec4(0, -1, 1, 2147483647) + u;
    gl_FragColor.r = r.x == 2 ? 1.0 : 0.0;
    gl_FragColor.g = r.y == 1 ? 1.0 : 0.0;
    gl_FragColor.b = r.z == 3 ? 1.0 : 0.0;
    gl_FragColor.a = r.w == -2147483647 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 2);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test ivec + ivec, ivec + int overflow. Expect wraparound.
TEST_P(GLSLUBTest, AddIvecIvecOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform ivec4 u;
void main() {
    ivec4 r0 = u + ivec4(0, -1, 1, 2147483647);
    ivec4 r1 = ivec4(0, -1, 1, 2147483647) + u;
    gl_FragColor.r = r0 == r1 ? 1.0 : 0.0;
    gl_FragColor.g = r0.y == 1 ? 1.0 : 0.0;
    gl_FragColor.b = r0.z == 3 ? 1.0 : 0.0;
    gl_FragColor.a = r0.w == -2147483647 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform4i(u, 2, 2, 2, 2);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test highp ivec += int overflow. Expect wraparound.
TEST_P(GLSLUBTest, AddAssignIvecIntOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    ivec4 r = ivec4(0, -1, 1, 2147483647);
    r += u;
    gl_FragColor.r = r.x == 2 ? 1.0 : 0.0;
    gl_FragColor.g = r.y == 1 ? 1.0 : 0.0;
    gl_FragColor.b = r.z == 3 ? 1.0 : 0.0;
    gl_FragColor.a = r.w == -2147483647 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 2);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test highp ivec += ivec overflow. Expect wraparound.
TEST_P(GLSLUBTest, AddAssignIvecIvecOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform ivec4 u;
void main() {
    ivec4 r = ivec4(0, -1, 1, 2147483647);
    r += u;
    gl_FragColor.r = r.x == 2 ? 1.0 : 0.0;
    gl_FragColor.g = r.y == 1 ? 1.0 : 0.0;
    gl_FragColor.b = r.z == 3 ? 1.0 : 0.0;
    gl_FragColor.a = r.w == -2147483647 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform4i(u, 2, 2, 2, 2);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int - int with overflow. Expect wraparound.
TEST_P(GLSLUBTest, SubIntIntOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    int r0 = u - (-1);
    int r1 = u - 0;
    int r2 = -2147483646 - u;
    int r3 = u - (-2147483647);

    gl_FragColor.r = r0 == 3 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == 2 ? 1.0 : 0.0;
    gl_FragColor.b = r2 == 2147483648 ? 1.0 : 0.0;
    gl_FragColor.a = r3 == -2147483647 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 2);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test highp int - ivec underflow. Expect wraparound.
TEST_P(GLSLUBTest, SubIntIvecUnderflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    ivec4 r = u - ivec4(0, -1, 1, 2147483647);
    gl_FragColor.r = r.x == 2 ? 1.0 : 0.0;
    gl_FragColor.g = r.y == 3 ? 1.0 : 0.0;
    gl_FragColor.b = r.z == 1 ? 1.0 : 0.0;
    gl_FragColor.a = r.w == -2147483645 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 2);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test highp ivec - int underflow. Expect wraparound.
TEST_P(GLSLUBTest, SubIvecIntUnderflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    ivec4 r = ivec4(0, -1, 1, -2147483647) - u;
    gl_FragColor.r = r.x == -2 ? 1.0 : 0.0;
    gl_FragColor.g = r.y == -3 ? 1.0 : 0.0;
    gl_FragColor.b = r.z == -1 ? 1.0 : 0.0;
    gl_FragColor.a = r.w == 2147483647 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 2);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test highp int vec -= scalar underflow. Expect wraparound.
TEST_P(GLSLUBTest, SubAssignIvecIntUnderflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    ivec4 r = ivec4(0, -1, 1, -2147483647);
    r -= u;
    gl_FragColor.r = r.x == -2 ? 1.0 : 0.0;
    gl_FragColor.g = r.y == -3 ? 1.0 : 0.0;
    gl_FragColor.b = r.z == -1 ? 1.0 : 0.0;
    gl_FragColor.a = r.w == 2147483647 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 2);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test highp int vec -= scalar underflow. Expect wraparound.
TEST_P(GLSLUBTest, SubAssignIvecIvecUnderflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform ivec4 u;
void main() {
    ivec4 r = ivec4(0, -1, 1, -2147483647);
    r -= u;
    gl_FragColor.r = r.x == -2 ? 1.0 : 0.0;
    gl_FragColor.g = r.y == -3 ? 1.0 : 0.0;
    gl_FragColor.b = r.z == -1 ? 1.0 : 0.0;
    gl_FragColor.a = r.w == 2147483647 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform4i(u, 2, 2, 2, 2);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test ++int with overflow. Expect wraparound.
TEST_P(GLSLUBTest, PreIncrementIntOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    int r0 = u;
    int r1 = ++r0;

    gl_FragColor.r = r0 == -2147483648 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == -2147483648 ? 1.0 : 0.0;
    gl_FragColor.b = u == 2147483647 ? 1.0 : 0.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 2147483647);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int++ with overflow. Expect wraparound.
TEST_P(GLSLUBTest, PostIncrementIntOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    int r0 = u;
    int r1 = r0++;

    gl_FragColor.r = r0 == -2147483648 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == 2147483647 ? 1.0 : 0.0;
    gl_FragColor.b = u == 2147483647 ? 1.0 : 0.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 2147483647);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test --int with overflow. Expect wraparound.
TEST_P(GLSLUBTest, PreDecrementIntOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    int r0 = u;
    int r1 = --r0;

    gl_FragColor.r = r0 == 2147483648 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == 2147483648 ? 1.0 : 0.0;
    gl_FragColor.b = u == -2147483647 ? 1.0 : 0.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, -2147483647);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int-- with overflow. Expect wraparound.
TEST_P(GLSLUBTest, PostDecrementIntOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    int r0 = u;
    int r1 = r0--;

    gl_FragColor.r = r0 == 2147483648 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == -2147483647 ? 1.0 : 0.0;
    gl_FragColor.b = u == -2147483647 ? 1.0 : 0.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, -2147483647);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test ++ivec with overflow. Expect wraparound.
TEST_P(GLSLUBTest, PreIncrementIvecOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform ivec4 u;
void main() {
    ivec4 r0 = u;
    ivec4 r1 = ++r0;

    gl_FragColor.r = r0 == ivec4(1, 2, 3, -2147483648) ? 1.0 : 0.0;
    gl_FragColor.g = r1 == ivec4(1, 2, 3, -2147483648) ? 1.0 : 0.0;
    gl_FragColor.b = u == ivec4(0, 1, 2, 2147483647) ? 1.0 : 0.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform4i(u, 0, 1, 2, 2147483647);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}
// Test ivec++ with overflow. Expect wraparound.
TEST_P(GLSLUBTest, PostIncrementIvecOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform ivec4 u;
void main() {
    ivec4 r0 = u;
    ivec4 r1 = r0++;

    gl_FragColor.r = r0 == ivec4(1, 2, 3, -2147483648) ? 1.0 : 0.0;
    gl_FragColor.g = r1 == ivec4(0, 1, 2, 2147483647) ? 1.0 : 0.0;
    gl_FragColor.b = u == ivec4(0, 1, 2, 2147483647) ? 1.0 : 0.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform4i(u, 0, 1, 2, 2147483647);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test --ivec with overflow. Expect wraparound.
TEST_P(GLSLUBTest, PreDecrementIvecOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform ivec4 u;
void main() {
    ivec4 r0 = u;
    ivec4 r1 = --r0;

    gl_FragColor.r = r0 == ivec4(-1, 0, 1, 2147483648) ? 1.0 : 0.0;
    gl_FragColor.g = r1 == ivec4(-1, 0, 1, 2147483648) ? 1.0 : 0.0;
    gl_FragColor.b = u == ivec4(0, 1, 2, -2147483647) ? 1.0 : 0.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform4i(u, 0, 1, 2, -2147483647);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test ivec-- with overflow. Expect wraparound.
TEST_P(GLSLUBTest, PostDecrementIvecOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform ivec4 u;
void main() {
    ivec4 r0 = u;
    ivec4 r1 = r0--;

    gl_FragColor.r = r0 == ivec4(-1, 0, 1, 2147483648) ? 1.0 : 0.0;
    gl_FragColor.g = r1 == ivec4(0, 1, 2, -2147483647) ? 1.0 : 0.0;
    gl_FragColor.b = u == ivec4(0, 1, 2, -2147483647) ? 1.0 : 0.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform4i(u, 0, 1, 2, -2147483647);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int++ with overflow. Expect wraparound.
TEST_P(GLSLUBTest, PostIncrementIntOverflowInForDynamic)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    int z = 0;
    for (int i = u; i > 4; i++) {
        z++;
    }
    gl_FragColor.r = z == 7 ? 1.0 : 0.0;
    gl_FragColor.g = u == 2147483641 ? 1.0 : 0.0;
    gl_FragColor.b = 1.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 2147483641);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int++ with overflow. Expect wraparound.
TEST_P(GLSLUBTest, PostIncrementIntOverflowInForStatic)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
void main() {
    int z = 0;
    for (int i = 2147483642; i > 4; i++) {
        z++;
    }
    gl_FragColor.r = z == 6 ? 1.0 : 0.0;
    gl_FragColor.g = 1.0;
    gl_FragColor.b = 1.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

}  // anonymous namespace

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLUBTest);
ANGLE_INSTANTIATE_TEST(GLSLUBTest, ES2_METAL(), ES3_METAL());
