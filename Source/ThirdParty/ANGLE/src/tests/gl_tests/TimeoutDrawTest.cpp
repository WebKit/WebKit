//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"
#include "util/shader_utils.h"

using namespace angle;

namespace
{
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
TEST_P(TimeoutDrawTest, TrivialInfiniteLoopVS)
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
    EXPECT_GL_ERROR(GL_CONTEXT_LOST);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);  // Should read through client buffer since context should be lost.
}

// Tests that trivial infinite loops in fragment shaders hang instead of progress.
TEST_P(TimeoutDrawTest, TrivialInfiniteLoopFS)
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
    EXPECT_GL_ERROR(GL_CONTEXT_LOST);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);  // Should read through client buffer since context should be lost.
}


// Tests that infinite loops based on user-supplied values in vertex shaders hang instead of progress.
// Otherwise optimizer would be able to assume something about the domain of the user-supplied value.
TEST_P(TimeoutDrawTest, DynamicInfiniteLoopVS)
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
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);  // Should read through client buffer since context should be lost.
    EXPECT_GL_ERROR(GL_CONTEXT_LOST);
}

// Tests that infinite loops based on user-supplied values in fragment shaders hang instead of progress.
// Otherwise optimizer would be able to assume something about the domain of the user-supplied value.
TEST_P(TimeoutDrawTest, DynamicInfiniteLoopFS)
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
    EXPECT_GL_ERROR(GL_CONTEXT_LOST);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);  // Should read through client buffer since context should be lost.
}

// Tests that infinite loops based on user-supplied values in vertex shaders hang instead of progress.
// Otherwise optimizer would be able to assume something about the domain of the user-supplied value.
// Explicit value break variant.
TEST_P(TimeoutDrawTest, DynamicInfiniteLoop2VS)
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
    EXPECT_GL_ERROR(GL_CONTEXT_LOST);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);  // Should read through client buffer since context should be lost.
}

// Tests that infinite loops based on user-supplied values in fragment shaders hang instead of progress.
// Otherwise optimizer would be able to assume something about the domain of the user-supplied value.
// Explicit value break variant.
TEST_P(TimeoutDrawTest, DynamicInfiniteLoop2FS)
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
    EXPECT_GL_ERROR(GL_CONTEXT_LOST);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);  // Should read through client buffer since context should be lost.
}

}

ANGLE_INSTANTIATE_TEST(TimeoutDrawTest,
                       WithRobustness(ES2_METAL().enable(Feature::InjectAsmStatementIntoLoopBodies)),
                       WithRobustness(ES3_METAL().enable(Feature::InjectAsmStatementIntoLoopBodies)));
