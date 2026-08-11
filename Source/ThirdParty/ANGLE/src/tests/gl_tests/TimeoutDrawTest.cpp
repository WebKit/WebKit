
//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "util/shader_utils.h"

namespace
{

// To inspect current behavior, run the tests in following manner:
// clang-format off
// ANGLE_FEATURE_OVERRIDES_DISABLED=injectAsmStatementIntoLoopBodies GMD_STDOUT=1 ./out/Debug/angle_end2end_tests --gtest_also_run_disabled_tests --gtest_filter=TimeoutDrawTest.DISABLED_DynamicInfiniteLoop2VS/ES3_Metal
// GMD_STDOUT=1 ./out/Debug/angle_end2end_tests --gtest_also_run_disabled_tests --gtest_filter=TimeoutDrawTest.DISABLED_DynamicInfiniteLoop2VS/ES3_Metal_EnsureLoopForwardProgress
// clang-format on

using namespace angle;
class TimeoutDrawTest : public ANGLETest<>
{
  protected:
    TimeoutDrawTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        // Tests should skip if robustness not supported, but this can be done only after
        // Metal supports robustness.
        if (IsEGLClientExtensionEnabled("EGL_EXT_create_context_robustness"))
        {
            setContextResetStrategy(EGL_LOSE_CONTEXT_ON_RESET_EXT);
        }
        else
        {
            setContextResetStrategy(EGL_NO_RESET_NOTIFICATION_EXT);
        }
    }
    void testSetUp() override
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glFinish();
    }
};

// Tests that trivial infinite loops in vertex shaders hang instead of progress.
TEST_P(TimeoutDrawTest, DISABLED_TrivialInfiniteLoopVS)
{
    constexpr char kVS[] = R"(precision highp float;
attribute vec4 a_position;
void main()
{
    for (;;) {}
    gl_Position = a_position;
})";
    ANGLE_GL_PROGRAM(program, kVS, essl1_shaders::fs::Red());
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    glFinish();
    if (glGetError() != GL_CONTEXT_LOST)
    {
        FAIL();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);
    }
}

// Tests that trivial infinite loops in fragment shaders hang instead of progress.
TEST_P(TimeoutDrawTest, DISABLED_TrivialInfiniteLoopFS)
{
    constexpr char kFS[] = R"(precision mediump float;
void main()
{
    for (;;) {}
    gl_FragColor = vec4(1, 0, 0, 1);
})";
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    glFinish();
    if (glGetError() != GL_CONTEXT_LOST)
    {
        FAIL();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);
    }
}
// Tests that infinite loops based on user-supplied values in vertex shaders hang instead of
// progress. Otherwise optimizer would be able to assume something about the domain of the
// user-supplied value.
TEST_P(TimeoutDrawTest, DISABLED_DynamicInfiniteLoopVS)
{
    constexpr char kVS[] = R"(precision highp float;
attribute vec4 a_position;
uniform int f;
void main()
{
    for (;f != 0;) {}
    gl_Position = a_position;
})";
    ANGLE_GL_PROGRAM(program, kVS, essl1_shaders::fs::Red());
    glUseProgram(program);
    GLint uniformLocation = glGetUniformLocation(program, "f");
    glUniform1i(uniformLocation, 77);
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    glFinish();
    if (glGetError() != GL_CONTEXT_LOST)
    {
        FAIL();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);
    }
}
// Tests that infinite loops based on user-supplied values in fragment shaders hang instead of
// progress. Otherwise optimizer would be able to assume something about the domain of the
// user-supplied value.
TEST_P(TimeoutDrawTest, DISABLED_DynamicInfiniteLoopFS)
{
    constexpr char kFS[] = R"(precision mediump float;
uniform int f;
void main()
{
    for (;f != 0;) {}
    gl_FragColor = vec4(1, 0, 0, 1);
})";
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    glUseProgram(program);
    GLint uniformLocation = glGetUniformLocation(program, "f");
    glUniform1i(uniformLocation, 88);
    EXPECT_GL_NO_ERROR();
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    glFinish();
    if (glGetError() != GL_CONTEXT_LOST)
    {
        FAIL();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);
    }
}
// Tests that infinite loops based on user-supplied values in vertex shaders hang instead of
// progress. Otherwise optimizer would be able to assume something about the domain of the
// user-supplied value. Explicit value break variant.
TEST_P(TimeoutDrawTest, DISABLED_DynamicInfiniteLoop2VS)
{
    constexpr char kVS[] = R"(precision highp float;
attribute vec4 a_position;
uniform int f;
void main()
{
    for (;;) { if (f <= 1) break; }
    gl_Position = a_position;
})";
    ANGLE_GL_PROGRAM(program, kVS, essl1_shaders::fs::Red());
    glUseProgram(program);
    GLint uniformLocation = glGetUniformLocation(program, "f");
    glUniform1i(uniformLocation, 66);
    EXPECT_GL_NO_ERROR();
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    glFinish();
    if (glGetError() != GL_CONTEXT_LOST)
    {
        FAIL();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);
    }
}

// Tests that infinite loops based on user-supplied values in fragment shaders hang instead of
// progress. Otherwise optimizer would be able to assume something about the domain of the
// user-supplied value. Explicit value break variant.
TEST_P(TimeoutDrawTest, DISABLED_DynamicInfiniteLoop2FS)
{
    constexpr char kFS[] = R"(precision mediump float;
uniform float f;
void main()
{
    for (;;) { if (f < 0.1) break; }
    gl_FragColor = vec4(1, 0, f, 1);
})";
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    glUseProgram(program);
    GLint uniformLocation = glGetUniformLocation(program, "f");
    glUniform1f(uniformLocation, .5f);
    EXPECT_GL_NO_ERROR();
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    glFinish();
    if (glGetError() != GL_CONTEXT_LOST)
    {
        FAIL();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);
    }
}
}  // namespace

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TimeoutDrawTest);

ANGLE_INSTANTIATE_TEST(
    TimeoutDrawTest,
    WithRobustness(ES2_METAL()),
    WithRobustness(ES3_METAL()),
    WithRobustness(ES2_METAL().enable(Feature::InjectAsmStatementIntoLoopBodies)),
    WithRobustness(ES3_METAL().enable(Feature::InjectAsmStatementIntoLoopBodies)),
    WithRobustness(ES2_METAL().enable(Feature::EnsureLoopForwardProgress)),
    WithRobustness(ES3_METAL().enable(Feature::EnsureLoopForwardProgress)));
