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

// Test abs() on the smallest negative int. The spec defines abs(x) as -x for negative x, and that
// negation overflows, so expect wraparound, i.e. the value is unchanged.
TEST_P(GLSLUBTest, AbsIntOverflow)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform int u;
uniform ivec4 uv;
out vec4 color;
void main() {
    int r0 = abs(u);
    ivec4 r1 = abs(uv);

    color.r = r0 == -2147483648 ? 1.0 : 0.0;
    color.g = r1 == ivec4(0, 1, 2147483647, -2147483648) ? 1.0 : 0.0;
    // abs() of a negative value is negative only when it overflows.
    color.b = abs(u) < 0 ? 1.0 : 0.0;
    color.a = abs(u) == u ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint uv = glGetUniformLocation(testProgram, "uv");
    EXPECT_NE(-1, uv);
    glUniform1i(u, -2147483647 - 1);
    glUniform4i(uv, 0, -1, -2147483647, -2147483647 - 1);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test unary - on the smallest negative int, which overflows. Expect wraparound, i.e. the value is
// unchanged. The comparisons are written so that a compiler that assumes the negation cannot
// overflow can fold them to a constant.
TEST_P(GLSLUBTest, NegateIntOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    int r0 = -u;
    int r1 = -(-u);

    gl_FragColor.r = r0 == -2147483648 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == -2147483648 ? 1.0 : 0.0;
    // -u == u only holds for 0 and for the smallest negative value.
    gl_FragColor.b = -u == u ? 1.0 : 0.0;
    // The negation of a negative value is negative only when it overflows.
    gl_FragColor.a = -u < 0 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, -2147483647 - 1);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test unary - on an ivec containing the smallest negative int. Expect wraparound.
TEST_P(GLSLUBTest, NegateIvecOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform ivec4 u;
void main() {
    ivec4 r0 = -u;
    ivec4 r1 = -(-u);

    gl_FragColor.r = r0 == ivec4(0, -1, -2147483647, -2147483648) ? 1.0 : 0.0;
    gl_FragColor.g = r1 == u ? 1.0 : 0.0;
    gl_FragColor.b = -u.w == u.w ? 1.0 : 0.0;
    gl_FragColor.a = -u.w < 0 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform4i(u, 0, 1, 2147483647, -2147483647 - 1);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int * int with overflow. Expect wraparound.
TEST_P(GLSLUBTest, MulIntIntOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    int r0 = u * u;
    int r1 = u * 32768;
    int r2 = 49152 * u;
    int r3 = u * -49152;

    gl_FragColor.r = r0 == 0 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == -2147483648 ? 1.0 : 0.0;
    gl_FragColor.b = r2 == -1073741824 ? 1.0 : 0.0;
    gl_FragColor.a = r3 == 1073741824 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 65536);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int * ivec with overflow. Expect wraparound.
TEST_P(GLSLUBTest, MulIntIvecOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    ivec4 r = u * ivec4(1, 32768, 49152, -49152);

    gl_FragColor.r = r.x == 65536 ? 1.0 : 0.0;
    gl_FragColor.g = r.y == -2147483648 ? 1.0 : 0.0;
    gl_FragColor.b = r.z == -1073741824 ? 1.0 : 0.0;
    gl_FragColor.a = r.w == 1073741824 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 65536);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test ivec * int with overflow. Expect wraparound.
TEST_P(GLSLUBTest, MulIvecIntOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    ivec4 r = ivec4(1, 32768, 49152, -49152) * u;

    gl_FragColor.r = r.x == 65536 ? 1.0 : 0.0;
    gl_FragColor.g = r.y == -2147483648 ? 1.0 : 0.0;
    gl_FragColor.b = r.z == -1073741824 ? 1.0 : 0.0;
    gl_FragColor.a = r.w == 1073741824 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 65536);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test ivec * ivec with overflow. Expect wraparound.
TEST_P(GLSLUBTest, MulIvecIvecOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform ivec4 u;
void main() {
    ivec4 r = u * u;

    gl_FragColor.r = r.x == 0 ? 1.0 : 0.0;
    gl_FragColor.g = r.y == 1073741824 ? 1.0 : 0.0;
    gl_FragColor.b = r.z == -1879048192 ? 1.0 : 0.0;
    gl_FragColor.a = r.w == -1879048192 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform4i(u, 65536, 32768, 49152, -49152);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int *= int with overflow. Expect wraparound.
TEST_P(GLSLUBTest, MulAssignIntIntOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    int r0 = u;
    r0 *= u;
    int r1 = 32768;
    r1 *= u;

    gl_FragColor.r = r0 == 0 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == -2147483648 ? 1.0 : 0.0;
    gl_FragColor.b = u == 65536 ? 1.0 : 0.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 65536);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test ivec *= int and ivec *= ivec with overflow. Expect wraparound.
TEST_P(GLSLUBTest, MulAssignIvecOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void main() {
    ivec4 r0 = ivec4(1, 32768, 49152, -49152);
    r0 *= u;
    ivec4 r1 = ivec4(1, 32768, 49152, -49152);
    r1 *= ivec4(u);

    gl_FragColor.r = r0 == ivec4(65536, -2147483648, -1073741824, 1073741824) ? 1.0 : 0.0;
    gl_FragColor.g = r0 == r1 ? 1.0 : 0.0;
    gl_FragColor.b = 1.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform1i(u, 65536);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int / int where the divisor is 0. GLSL ES 3.00 section 5.9 makes this an unspecified value,
// so the value asserted here is only what the emulation happens to produce, which is the dividend.
// The well defined divisions must not be affected.
TEST_P(GLSLUBTest, DivIntIntByZero)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
uniform int z;
void main() {
    int r0 = u / z;
    int r1 = -u / z;
    int r2 = u / 2;
    int r3 = z / u;

    gl_FragColor.r = r0 == 7 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == -7 ? 1.0 : 0.0;
    gl_FragColor.b = r2 == 3 ? 1.0 : 0.0;
    gl_FragColor.a = r3 == 0 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint z = glGetUniformLocation(testProgram, "z");
    EXPECT_NE(-1, z);
    glUniform1i(u, 7);
    glUniform1i(z, 0);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test the int / int case that overflows, i.e. dividing the smallest negative value by -1. The
// spec allows either the smallest or the largest representable value to be returned here (GLSL ES
// 3.00 section 4.1.3), and the emulation returns the smallest, which is what is asserted.
TEST_P(GLSLUBTest, DivIntIntOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
uniform int m;
void main() {
    int r0 = u / m;
    int r1 = u / 2;
    int r2 = (u + 1) / m;
    int r3 = u / 1;

    gl_FragColor.r = r0 == -2147483648 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == -1073741824 ? 1.0 : 0.0;
    gl_FragColor.b = r2 == 2147483647 ? 1.0 : 0.0;
    gl_FragColor.a = r3 == -2147483648 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint m = glGetUniformLocation(testProgram, "m");
    EXPECT_NE(-1, m);
    glUniform1i(u, -2147483647 - 1);
    glUniform1i(m, -1);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test ivec / ivec, ivec / int and ivec /= int with a 0 divisor and with an overflow.
TEST_P(GLSLUBTest, DivIvecByZero)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform ivec4 u;
uniform ivec4 zv;
uniform int z;
void main() {
    ivec4 r0 = u / zv;
    ivec4 r1 = u / z;
    ivec4 r2 = u;
    r2 /= z;

    gl_FragColor.r = r0 == ivec4(7, -7, 0, -2147483648) ? 1.0 : 0.0;
    gl_FragColor.g = r1 == u ? 1.0 : 0.0;
    gl_FragColor.b = r2 == u ? 1.0 : 0.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint zv = glGetUniformLocation(testProgram, "zv");
    EXPECT_NE(-1, zv);
    GLint z = glGetUniformLocation(testProgram, "z");
    EXPECT_NE(-1, z);
    glUniform4i(u, 7, -7, 0, -2147483647 - 1);
    glUniform4i(zv, 0, 0, 0, -1);
    glUniform1i(z, 0);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int /= int where the divisor is 0.
TEST_P(GLSLUBTest, DivAssignIntIntByZero)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
uniform int z;
void main() {
    int r0 = u;
    r0 /= z;
    int r1 = u;
    r1 /= 2;

    gl_FragColor.r = r0 == 7 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == 3 ? 1.0 : 0.0;
    gl_FragColor.b = 1.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint z = glGetUniformLocation(testProgram, "z");
    EXPECT_NE(-1, z);
    glUniform1i(u, 7);
    glUniform1i(z, 0);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test uint / uint where the divisor is 0.
TEST_P(GLSLUBTest, DivUintUintByZero)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform uint u;
uniform uint z;
out vec4 color;
void main() {
    uint r0 = u / z;
    uint r1 = u / 2u;
    uvec4 r2 = uvec4(u) / uvec4(z);

    color.r = r0 == 7u ? 1.0 : 0.0;
    color.g = r1 == 3u ? 1.0 : 0.0;
    color.b = r2 == uvec4(7u) ? 1.0 : 0.0;
    color.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint z = glGetUniformLocation(testProgram, "z");
    EXPECT_NE(-1, z);
    glUniform1ui(u, 7u);
    glUniform1ui(z, 0u);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int % int where the divisor is 0. GLSL ES 3.00 section 5.9 leaves the result undefined per
// component, so the value asserted here is only what the emulation produces, which is 0. The well
// defined remainders must not be affected.
TEST_P(GLSLUBTest, ModIntIntByZero)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform int u;
uniform int z;
out vec4 color;
void main() {
    int r0 = u % z;
    int r1 = -u % z;
    int r2 = u % 2;
    int r3 = z % u;

    color.r = r0 == 0 ? 1.0 : 0.0;
    color.g = r1 == 0 ? 1.0 : 0.0;
    color.b = r2 == 1 ? 1.0 : 0.0;
    color.a = r3 == 0 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint z = glGetUniformLocation(testProgram, "z");
    EXPECT_NE(-1, z);
    glUniform1i(u, 7);
    glUniform1i(z, 0);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test the int % int case where the corresponding division overflows, i.e. the smallest negative
// value modulo -1. GLSL ES 3.00 section 5.9 leaves the result undefined because an operand is
// negative, and the emulation returns the mathematically correct 0, which is what is asserted.
TEST_P(GLSLUBTest, ModIntIntOverflow)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform int u;
uniform int m;
out vec4 color;
void main() {
    int r0 = u % m;
    int r1 = u % 2;
    int r2 = (u + 1) % m;
    int r3 = u % 1;

    color.r = r0 == 0 ? 1.0 : 0.0;
    color.g = r1 == 0 ? 1.0 : 0.0;
    color.b = r2 == 0 ? 1.0 : 0.0;
    color.a = r3 == 0 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint m = glGetUniformLocation(testProgram, "m");
    EXPECT_NE(-1, m);
    glUniform1i(u, -2147483647 - 1);
    glUniform1i(m, -1);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int % int with negative operands, which GLSL ES 3.00 section 5.9 leaves undefined. The
// emulation truncates towards zero, so the sign follows the dividend, which is what is asserted.
TEST_P(GLSLUBTest, ModIntIntNegative)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform int a;
uniform int b;
out vec4 color;
void main() {
    int r0 = -a % b;
    int r1 = a % -b;
    int r2 = -a % -b;
    int r3 = a % b;

    color.r = r0 == -1 ? 1.0 : 0.0;
    color.g = r1 == 1 ? 1.0 : 0.0;
    color.b = r2 == -1 ? 1.0 : 0.0;
    color.a = r3 == 1 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint a = glGetUniformLocation(testProgram, "a");
    EXPECT_NE(-1, a);
    GLint b = glGetUniformLocation(testProgram, "b");
    EXPECT_NE(-1, b);
    glUniform1i(a, 7);
    glUniform1i(b, 3);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test ivec % ivec, ivec % int and ivec %= int with a 0 divisor and with an overflow.
TEST_P(GLSLUBTest, ModIvecByZero)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform ivec4 u;
uniform ivec4 zv;
uniform int z;
out vec4 color;
void main() {
    ivec4 r0 = u % zv;
    ivec4 r1 = u % z;
    ivec4 r2 = u;
    r2 %= z;

    color.r = r0 == ivec4(0) ? 1.0 : 0.0;
    color.g = r1 == ivec4(0) ? 1.0 : 0.0;
    color.b = r2 == ivec4(0) ? 1.0 : 0.0;
    color.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint zv = glGetUniformLocation(testProgram, "zv");
    EXPECT_NE(-1, zv);
    GLint z = glGetUniformLocation(testProgram, "z");
    EXPECT_NE(-1, z);
    glUniform4i(u, 7, -7, 0, -2147483647 - 1);
    glUniform4i(zv, 0, 0, 0, -1);
    glUniform1i(z, 0);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test uint % uint where the divisor is 0.
TEST_P(GLSLUBTest, ModUintUintByZero)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform uint u;
uniform uint z;
out vec4 color;
void main() {
    uint r0 = u % z;
    uint r1 = u % 2u;
    uvec4 r2 = uvec4(u);
    r2 %= z;

    color.r = r0 == 0u ? 1.0 : 0.0;
    color.g = r1 == 1u ? 1.0 : 0.0;
    color.b = r2 == uvec4(0u) ? 1.0 : 0.0;
    color.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint z = glGetUniformLocation(testProgram, "z");
    EXPECT_NE(-1, z);
    glUniform1ui(u, 7u);
    glUniform1ui(z, 0u);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int << int with a shift amount that is out of range. The result is undefined, so this only
// tests that the shader runs. The emulation masks the shift amount to 5 bits, so shifting by 32
// shifts by 0 and shifting by -1 shifts by 31.
TEST_P(GLSLUBTest, ShiftLeftIntAmountOutOfRange)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform int u;
uniform int big;
uniform int neg;
out vec4 color;
void main() {
    int r0 = u << big;
    int r1 = u << neg;
    int r2 = u << 31;
    int r3 = u << 0;

    color.r = r0 == 1 ? 1.0 : 0.0;
    color.g = r1 == -2147483648 ? 1.0 : 0.0;
    color.b = r2 == -2147483648 ? 1.0 : 0.0;
    color.a = r3 == 1 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint big = glGetUniformLocation(testProgram, "big");
    EXPECT_NE(-1, big);
    GLint neg = glGetUniformLocation(testProgram, "neg");
    EXPECT_NE(-1, neg);
    glUniform1i(u, 1);
    glUniform1i(big, 32);
    glUniform1i(neg, -1);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int << int where the value is negative or where the shift overflows. Expect the bits to be
// shifted out.
TEST_P(GLSLUBTest, ShiftLeftIntNegativeValue)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform int u;
uniform int h;
out vec4 color;
void main() {
    int r0 = u << 1;
    int r1 = u << 31;
    int r2 = h << 1;
    int r3 = h << 2;

    color.r = r0 == -2 ? 1.0 : 0.0;
    color.g = r1 == -2147483648 ? 1.0 : 0.0;
    color.b = r2 == -2147483648 ? 1.0 : 0.0;
    color.a = r3 == 0 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint h = glGetUniformLocation(testProgram, "h");
    EXPECT_NE(-1, h);
    glUniform1i(u, -1);
    glUniform1i(h, 1073741824);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int >> int with a shift amount that is out of range. The result is undefined, so this only
// tests that the shader runs. The emulation masks the shift amount to 5 bits. The in range shifts
// must sign extend.
TEST_P(GLSLUBTest, ShiftRightIntAmountOutOfRange)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform int u;
uniform int big;
uniform int neg;
out vec4 color;
void main() {
    int r0 = u >> big;
    int r1 = u >> neg;
    int r2 = u >> 1;
    int r3 = u >> 31;

    color.r = r0 == -8 ? 1.0 : 0.0;
    color.g = r1 == -1 ? 1.0 : 0.0;
    color.b = r2 == -4 ? 1.0 : 0.0;
    color.a = r3 == -1 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint big = glGetUniformLocation(testProgram, "big");
    EXPECT_NE(-1, big);
    GLint neg = glGetUniformLocation(testProgram, "neg");
    EXPECT_NE(-1, neg);
    glUniform1i(u, -8);
    glUniform1i(big, 32);
    glUniform1i(neg, -1);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test uint << uint and uint >> uint with a shift amount that is out of range.
TEST_P(GLSLUBTest, ShiftUintAmountOutOfRange)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform uint u;
uniform uint big;
out vec4 color;
void main() {
    uint r0 = u << big;
    uint r1 = u >> big;
    uint r2 = u << 31u;
    uint r3 = u >> 31u;

    color.r = r0 == 4294967295u ? 1.0 : 0.0;
    color.g = r1 == 4294967295u ? 1.0 : 0.0;
    color.b = r2 == 2147483648u ? 1.0 : 0.0;
    color.a = r3 == 1u ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint big = glGetUniformLocation(testProgram, "big");
    EXPECT_NE(-1, big);
    glUniform1ui(u, 4294967295u);
    glUniform1ui(big, 32u);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test int <<= int and int >>= int with a shift amount that is out of range.
TEST_P(GLSLUBTest, ShiftAssignIntAmountOutOfRange)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform int u;
uniform int big;
out vec4 color;
void main() {
    int r0 = u;
    r0 <<= big;
    int r1 = u;
    r1 >>= big;
    int r2 = u;
    r2 <<= 1;
    int r3 = u;
    r3 >>= 1;

    color.r = r0 == -8 ? 1.0 : 0.0;
    color.g = r1 == -8 ? 1.0 : 0.0;
    color.b = r2 == -16 ? 1.0 : 0.0;
    color.a = r3 == -4 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint big = glGetUniformLocation(testProgram, "big");
    EXPECT_NE(-1, big);
    glUniform1i(u, -8);
    glUniform1i(big, 32);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test ivec << ivec, ivec << int and ivec <<= int with shift amounts that are out of range.
TEST_P(GLSLUBTest, ShiftLeftIvecAmountOutOfRange)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform ivec4 u;
uniform ivec4 amounts;
uniform int big;
out vec4 color;
void main() {
    ivec4 r0 = u << amounts;
    ivec4 r1 = u << big;
    ivec4 r2 = u;
    r2 <<= big;

    color.r = r0 == ivec4(1, 2, 1, -2147483648) ? 1.0 : 0.0;
    color.g = r1 == ivec4(1) ? 1.0 : 0.0;
    color.b = r2 == ivec4(1) ? 1.0 : 0.0;
    color.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint amounts = glGetUniformLocation(testProgram, "amounts");
    EXPECT_NE(-1, amounts);
    GLint big = glGetUniformLocation(testProgram, "big");
    EXPECT_NE(-1, big);
    glUniform4i(u, 1, 1, 1, 1);
    glUniform4i(amounts, 0, 1, 32, -1);
    glUniform1i(big, 64);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test float to int conversion where the value is out of range. GLSL ES 3.00 section 5.4.1 only
// specifies that the fractional part is dropped and does not define a result for values that are
// not representable, so the values asserted here are what the emulation produces: it clamps to the
// representable range.
TEST_P(GLSLUBTest, FloatToIntOutOfRange)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform float hi;
uniform float lo;
void main() {
    int r0 = int(hi);
    int r1 = int(lo);
    int r2 = int(2.7);
    int r3 = int(-2.7);

    gl_FragColor.r = r0 == 2147483520 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == -2147483648 ? 1.0 : 0.0;
    gl_FragColor.b = r2 == 2 ? 1.0 : 0.0;
    gl_FragColor.a = r3 == -2 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint hi = glGetUniformLocation(testProgram, "hi");
    EXPECT_NE(-1, hi);
    GLint lo = glGetUniformLocation(testProgram, "lo");
    EXPECT_NE(-1, lo);
    glUniform1f(hi, 1.0e10f);
    glUniform1f(lo, -1.0e10f);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test float to int conversion of the values at the edges of the int range. Unlike the out of range
// cases, these are representable, so GLSL ES 3.00 section 5.4.1 requires the exact values and the
// clamping must not perturb them.
TEST_P(GLSLUBTest, FloatToIntInRange)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform float hi;
uniform float lo;
void main() {
    int r0 = int(hi);
    int r1 = int(lo);

    gl_FragColor.r = r0 == 2147483520 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == -2147483648 ? 1.0 : 0.0;
    gl_FragColor.b = 1.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint hi = glGetUniformLocation(testProgram, "hi");
    EXPECT_NE(-1, hi);
    GLint lo = glGetUniformLocation(testProgram, "lo");
    EXPECT_NE(-1, lo);
    // The largest float that is smaller than 2^31, and -2^31.
    glUniform1f(hi, 2147483520.0f);
    glUniform1f(lo, -2147483648.0f);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test float to uint conversion where the value is out of range. Note that GLSL ES 3.00 section
// 5.4.1 does explicitly make conversion of a negative value to uint undefined.
TEST_P(GLSLUBTest, FloatToUintOutOfRange)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform float hi;
uniform float lo;
out vec4 color;
void main() {
    uint r0 = uint(hi);
    uint r1 = uint(lo);
    uint r2 = uint(2.7);
    uvec2 r3 = uvec2(vec2(hi, lo));

    color.r = r0 == 4294967040u ? 1.0 : 0.0;
    color.g = r1 == 0u ? 1.0 : 0.0;
    color.b = r2 == 2u ? 1.0 : 0.0;
    color.a = r3 == uvec2(4294967040u, 0u) ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint hi = glGetUniformLocation(testProgram, "hi");
    EXPECT_NE(-1, hi);
    GLint lo = glGetUniformLocation(testProgram, "lo");
    EXPECT_NE(-1, lo);
    glUniform1f(hi, 1.0e10f);
    glUniform1f(lo, -1.0e10f);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test vec4 to ivec4 conversion where the values are out of range. The asserted values are what
// the emulation clamps to, see FloatToIntOutOfRange.
TEST_P(GLSLUBTest, FloatToIvecOutOfRange)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform vec4 u;
void main() {
    ivec4 r = ivec4(u);

    gl_FragColor.r = r.x == 2147483520 ? 1.0 : 0.0;
    gl_FragColor.g = r.y == -2147483648 ? 1.0 : 0.0;
    gl_FragColor.b = r.z == 2 ? 1.0 : 0.0;
    gl_FragColor.a = r.w == -2 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform4f(u, 1.0e10f, -1.0e10f, 2.7f, -2.7f);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test a multi argument ivec constructor where the values are out of range. These are split into
// per component conversions before being emitted. The asserted values are what the emulation clamps
// to, see FloatToIntOutOfRange.
TEST_P(GLSLUBTest, FloatToIvecMultiArgumentOutOfRange)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform float a;
uniform float b;
uniform vec2 v;
void main() {
    ivec2 r0 = ivec2(a, b);
    ivec4 r1 = ivec4(v, v);
    ivec3 r2 = ivec3(a, v);

    gl_FragColor.r = r0 == ivec2(2147483520, -2147483648) ? 1.0 : 0.0;
    gl_FragColor.g = r1 == ivec4(2147483520, -2147483648, 2147483520, -2147483648) ? 1.0 : 0.0;
    gl_FragColor.b = r2 == ivec3(2147483520, 2147483520, -2147483648) ? 1.0 : 0.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint a = glGetUniformLocation(testProgram, "a");
    EXPECT_NE(-1, a);
    GLint b = glGetUniformLocation(testProgram, "b");
    EXPECT_NE(-1, b);
    GLint v = glGetUniformLocation(testProgram, "v");
    EXPECT_NE(-1, v);
    glUniform1f(a, 1.0e10f);
    glUniform1f(b, -1.0e10f);
    glUniform2f(v, 1.0e10f, -1.0e10f);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test ++ and -- on a vector element with a dynamic index. Expect wraparound. The operand of the
// emulated operators has to be addressable.
TEST_P(GLSLUBTest, IncrementIvecElementOverflow)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform ivec4 u;
uniform int i;
out vec4 color;
void main() {
    ivec4 r0 = u;
    r0[i]++;
    ivec4 r1 = u;
    ++r1[i];
    ivec4 r2 = u;
    r2[i + 1]--;
    ivec4 r3 = u;
    --r3[i + 1];

    color.r = r0 == ivec4(0, 1, -2147483648, -2147483648) ? 1.0 : 0.0;
    color.g = r0 == r1 ? 1.0 : 0.0;
    color.b = r2 == ivec4(0, 1, 2147483647, 2147483647) ? 1.0 : 0.0;
    color.a = r2 == r3 ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint i = glGetUniformLocation(testProgram, "i");
    EXPECT_NE(-1, i);
    glUniform4i(u, 0, 1, 2147483647, -2147483647 - 1);
    glUniform1i(i, 2);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test ++ and -- on a swizzle. Expect wraparound. The operand of the emulated operators has to be
// addressable.
TEST_P(GLSLUBTest, IncrementIvecSwizzleOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform ivec4 u;
void main() {
    ivec4 r0 = u;
    r0.z++;
    ivec4 r1 = u;
    r1.zw--;
    ivec4 r2 = u;
    --r2.wz;

    gl_FragColor.r = r0 == ivec4(0, 1, -2147483648, -2147483648) ? 1.0 : 0.0;
    gl_FragColor.g = r1 == ivec4(0, 1, 2147483646, 2147483647) ? 1.0 : 0.0;
    gl_FragColor.b = r1 == r2 ? 1.0 : 0.0;
    gl_FragColor.a = 1.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl1_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    glUniform4i(u, 0, 1, 2147483647, -2147483647 - 1);
    drawQuad(testProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test compound assignment to a vector element and to a swizzle. Expect wraparound.
TEST_P(GLSLUBTest, AssignIvecElementOverflow)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
precision highp float;
uniform ivec4 u;
uniform int i;
out vec4 color;
void main() {
    ivec4 r0 = u;
    r0[i] += 2;
    ivec4 r1 = u;
    r1.z -= -2;
    ivec4 r2 = u;
    r2.zw *= 2;
    ivec4 r3 = u;
    r3[i] <<= 33;

    color.r = r0 == ivec4(0, 1, -2147483647, -2147483648) ? 1.0 : 0.0;
    color.g = r0 == r1 ? 1.0 : 0.0;
    color.b = r2 == ivec4(0, 1, -2, 0) ? 1.0 : 0.0;
    // The shift amount is masked, so shifting by 33 shifts by 1.
    color.a = r3 == ivec4(0, 1, -2, -2147483648) ? 1.0 : 0.0;
}
)";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
    glUseProgram(testProgram);
    GLint u = glGetUniformLocation(testProgram, "u");
    EXPECT_NE(-1, u);
    GLint i = glGetUniformLocation(testProgram, "i");
    EXPECT_NE(-1, i);
    glUniform4i(u, 0, 1, 2147483647, -2147483647 - 1);
    glUniform1i(i, 2);
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(255, 255, 255, 255));
    ASSERT_GL_NO_ERROR();
}

// Test that the emulated operators work on function out and inout arguments.
TEST_P(GLSLUBTest, OutArgumentOverflow)
{
    constexpr char kFS[] = R"(
precision highp int;
precision highp float;
uniform int u;
void addOne(inout int x) { x++; }
void mulTwo(inout ivec2 x) { x *= 2; }
void main() {
    int r0 = u;
    addOne(r0);
    ivec4 r1 = ivec4(u, u, 1, 2);
    addOne(r1[2]);
    ivec4 r2 = ivec4(u, u, 1, 2);
    mulTwo(r2.xy);

    gl_FragColor.r = r0 == -2147483648 ? 1.0 : 0.0;
    gl_FragColor.g = r1 == ivec4(2147483647, 2147483647, 2, 2) ? 1.0 : 0.0;
    gl_FragColor.b = r2 == ivec4(-2, -2, 1, 2) ? 1.0 : 0.0;
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

}  // anonymous namespace

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLUBTest);
ANGLE_INSTANTIATE_TEST(GLSLUBTest, ES2_METAL(), ES3_METAL());
