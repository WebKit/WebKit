//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

constexpr int kPixelColorThreshhold = 8;

class AdvancedBlendTest : public ANGLETest<>
{
  protected:
    AdvancedBlendTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Test that when blending is disabled, advanced blend is not applied.
// Regression test for a bug in the emulation path in the Vulkan backend.
TEST_P(AdvancedBlendTest, AdvancedBlendNotAppliedWhenBlendIsDisabled)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_KHR_blend_equation_advanced"));

    const char *vertSrc = R"(#version 320 es
        in highp vec4 a_position;
        in mediump vec4 a_color;
        out mediump vec4 v_color;
        void main()
        {
            gl_Position = a_position;
            v_color = a_color;
        }
    )";

    const char *fragSrc = R"(#version 320 es
        in mediump vec4 v_color;
        layout(blend_support_colorburn) out;
        layout(location = 0) out mediump vec4 o_color;
        void main()
        {
            o_color = v_color;
        }
    )";

    ANGLE_GL_PROGRAM(program, vertSrc, fragSrc);
    glUseProgram(program);

    std::array<GLfloat, 16> attribPosData = {1, 1,  0.5, 1, -1, 1,  0.5, 1,
                                             1, -1, 0.5, 1, -1, -1, 0.5, 1};

    GLint attribPosLoc = glGetAttribLocation(1, "a_position");
    ASSERT(attribPosLoc >= 0);
    glEnableVertexAttribArray(attribPosLoc);
    glVertexAttribPointer(attribPosLoc, 4, GL_FLOAT, GL_FALSE, 0, attribPosData.data());

    std::array<GLfloat, 16> attribColorData1 = {1, 0.2, 0.5, 1, 1, 0.2, 0.5, 1,
                                                1, 0.2, 0.5, 1, 1, 0.2, 0.5, 1};
    GLint attribColorLoc                     = glGetAttribLocation(1, "a_color");
    ASSERT(attribColorLoc >= 0);
    glEnableVertexAttribArray(attribColorLoc);
    glVertexAttribPointer(attribColorLoc, 4, GL_FLOAT, GL_FALSE, 0, attribColorData1.data());

    glBlendEquation(GL_COLORBURN);

    const uint16_t indices[] = {0, 1, 2, 2, 1, 3};
    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Disable the blend. The next glDrawElements() should not blend the a_color with clear color
    glDisable(GL_BLEND);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, &indices[0]);
    EXPECT_PIXEL_COLOR_NEAR(64, 64, GLColor(255, 51, 128, 255), kPixelColorThreshhold);
}

// Test that when blending is disabled, advanced blend is not applied, but is applied after
// it is enabled.
// Regression test for a bug in the emulation path in the Vulkan backend.
TEST_P(AdvancedBlendTest, AdvancedBlendDisabledAndThenEnabled)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_KHR_blend_equation_advanced"));

    const char *vertSrc = R"(#version 320 es
        in highp vec4 a_position;
        in mediump vec4 a_color;
        out mediump vec4 v_color;
        void main()
        {
            gl_Position = a_position;
            v_color = a_color;
        }
    )";

    const char *fragSrc = R"(#version 320 es
        in mediump vec4 v_color;
        layout(blend_support_colorburn) out;
        layout(location = 0) out mediump vec4 o_color;
        void main()
        {
            o_color = v_color;
        }
    )";

    ANGLE_GL_PROGRAM(program, vertSrc, fragSrc);
    glUseProgram(program);

    std::array<GLfloat, 16> attribPosData = {1, 1,  0.5, 1, -1, 1,  0.5, 1,
                                             1, -1, 0.5, 1, -1, -1, 0.5, 1};

    GLint attribPosLoc = glGetAttribLocation(1, "a_position");
    ASSERT(attribPosLoc >= 0);
    glEnableVertexAttribArray(attribPosLoc);
    glVertexAttribPointer(attribPosLoc, 4, GL_FLOAT, GL_FALSE, 0, attribPosData.data());

    std::array<GLfloat, 16> attribColorData1 = {1, 0.2, 0.5, 1, 1, 0.2, 0.5, 1,
                                                1, 0.2, 0.5, 1, 1, 0.2, 0.5, 1};
    GLint attribColorLoc                     = glGetAttribLocation(1, "a_color");
    ASSERT(attribColorLoc >= 0);
    glEnableVertexAttribArray(attribColorLoc);
    glVertexAttribPointer(attribColorLoc, 4, GL_FLOAT, GL_FALSE, 0, attribColorData1.data());

    glBlendEquation(GL_COLORBURN);

    const uint16_t indices[] = {0, 1, 2, 2, 1, 3};
    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Disable the blend. The next glDrawElements() should not blend the a_color with clear color
    glDisable(GL_BLEND);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, &indices[0]);

    // Enable the blend. The next glDrawElements() should blend a_color
    // with the the existing framebuffer output with GL_COLORBURN blend mode
    glEnable(GL_BLEND);
    // Test the blend with coherent blend disabled. This make the test cover both devices that
    // support / do not support GL_KHR_blend_equation_advanced_coherent
    if (IsGLExtensionEnabled("GL_KHR_blend_equation_advanced_coherent"))
    {
        glDisable(GL_BLEND_ADVANCED_COHERENT_KHR);
    }
    glBlendBarrier();
    std::array<GLfloat, 16> attribColorData2 = {0.5, 0.5, 0, 1, 0.5, 0.5, 0, 1,
                                                0.5, 0.5, 0, 1, 0.5, 0.5, 0, 1};
    glEnableVertexAttribArray(attribColorLoc);
    glVertexAttribPointer(attribColorLoc, 4, GL_FLOAT, GL_FALSE, 0, attribColorData2.data());
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, &indices[0]);

    EXPECT_PIXEL_COLOR_NEAR(64, 64, GLColor(255, 0, 0, 255), kPixelColorThreshhold);
}

// Test that when blending is enabled, advanced blend is applied, but is not applied after
// it is disabled.
// Regression test for a bug in the emulation path in the Vulkan backend.
TEST_P(AdvancedBlendTest, AdvancedBlendEnabledAndThenDisabled)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_KHR_blend_equation_advanced"));

    const char *vertSrc = R"(#version 320 es
        in highp vec4 a_position;
        in mediump vec4 a_color;
        out mediump vec4 v_color;
        void main()
        {
            gl_Position = a_position;
            v_color = a_color;
        }
    )";

    const char *fragSrc = R"(#version 320 es
        in mediump vec4 v_color;
        layout(blend_support_colorburn) out;
        layout(location = 0) out mediump vec4 o_color;
        void main()
        {
            o_color = v_color;
        }
    )";

    ANGLE_GL_PROGRAM(program, vertSrc, fragSrc);
    glUseProgram(program);

    std::array<GLfloat, 16> attribPosData = {1, 1,  0.5, 1, -1, 1,  0.5, 1,
                                             1, -1, 0.5, 1, -1, -1, 0.5, 1};

    GLint attribPosLoc = glGetAttribLocation(1, "a_position");
    ASSERT(attribPosLoc >= 0);
    glEnableVertexAttribArray(attribPosLoc);
    glVertexAttribPointer(attribPosLoc, 4, GL_FLOAT, GL_FALSE, 0, attribPosData.data());

    std::array<GLfloat, 16> attribColorData1 = {1, 0.2, 0.5, 1, 1, 0.2, 0.5, 1,
                                                1, 0.2, 0.5, 1, 1, 0.2, 0.5, 1};
    GLint attribColorLoc                     = glGetAttribLocation(1, "a_color");
    ASSERT(attribColorLoc >= 0);
    glEnableVertexAttribArray(attribColorLoc);
    glVertexAttribPointer(attribColorLoc, 4, GL_FLOAT, GL_FALSE, 0, attribColorData1.data());

    glBlendEquation(GL_COLORBURN);

    const uint16_t indices[] = {0, 1, 2, 2, 1, 3};
    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Enable the blend. The next glDrawElements() should blend the a_color with clear color
    // using the GL_COLORBURN blend mode
    glEnable(GL_BLEND);
    // Test the blend with coherent blend disabled. This make the test cover both devices that
    // support / do not support GL_KHR_blend_equation_advanced_coherent
    if (IsGLExtensionEnabled("GL_KHR_blend_equation_advanced_coherent"))
    {
        glDisable(GL_BLEND_ADVANCED_COHERENT_KHR);
    }
    glBlendBarrier();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, &indices[0]);

    // Disable the blend. The next glDrawElements() should not blend the a_color with
    // the existing framebuffer output with GL_COLORBURN blend mode
    glDisable(GL_BLEND);
    std::array<GLfloat, 16> attribColorData2 = {0.5, 0.5, 0, 1, 0.5, 0.5, 0, 1,
                                                0.5, 0.5, 0, 1, 0.5, 0.5, 0, 1};
    glEnableVertexAttribArray(attribColorLoc);
    glVertexAttribPointer(attribColorLoc, 4, GL_FLOAT, GL_FALSE, 0, attribColorData2.data());
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, &indices[0]);

    EXPECT_PIXEL_COLOR_NEAR(64, 64, GLColor(128, 128, 0, 255), kPixelColorThreshhold);
}

// Test querying advanced blend equation coherent on supported devices (enabled by default).
TEST_P(AdvancedBlendTest, AdvancedBlendCoherentQuery)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_KHR_blend_equation_advanced_coherent"));

    GLint status = -1;
    glGetIntegerv(GL_BLEND_ADVANCED_COHERENT_KHR, &status);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(status, 1);

    glDisable(GL_BLEND_ADVANCED_COHERENT_KHR);
    glGetIntegerv(GL_BLEND_ADVANCED_COHERENT_KHR, &status);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(status, 0);

    glEnable(GL_BLEND_ADVANCED_COHERENT_KHR);
    glGetIntegerv(GL_BLEND_ADVANCED_COHERENT_KHR, &status);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(status, 1);
}

// Test that querying advanced blend equation coherent results in an error as if this enum does not
// exist.
TEST_P(AdvancedBlendTest, AdvancedBlendCoherentQueryFailsIfNotSupported)
{
    ANGLE_SKIP_TEST_IF(IsGLExtensionEnabled("GL_KHR_blend_equation_advanced_coherent"));

    GLint status = -1;
    glGetIntegerv(GL_BLEND_ADVANCED_COHERENT_KHR, &status);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AdvancedBlendTest);
ANGLE_INSTANTIATE_TEST_ES32(AdvancedBlendTest);
