//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Test cases for GL_EXT_clip_control
// These tests complement dEQP-GLES2.functional.clip_control.*
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"
#include "util/test_utils.h"

using namespace angle;

class ClipControlTest : public ANGLETest<>
{
  protected:
    static const int w = 64;
    static const int h = 64;

    ClipControlTest()
    {
        setWindowWidth(w);
        setWindowHeight(h);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
        setExtensionsEnabled(false);
    }
};

// Test state queries and updates
TEST_P(ClipControlTest, StateQuery)
{
    // Queries with the extension disabled
    GLint clipOrigin = -1;
    glGetIntegerv(GL_CLIP_ORIGIN_EXT, &clipOrigin);
    EXPECT_EQ(clipOrigin, -1);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    GLint clipDepthMode = -1;
    glGetIntegerv(GL_CLIP_DEPTH_MODE_EXT, &clipDepthMode);
    EXPECT_EQ(clipDepthMode, -1);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Command with the extension disabled
    glClipControlEXT(GL_UPPER_LEFT_EXT, GL_ZERO_TO_ONE_EXT);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_clip_control"));
    ASSERT_GL_NO_ERROR();

    // Default state with the extension enabled
    glGetIntegerv(GL_CLIP_ORIGIN_EXT, &clipOrigin);
    glGetIntegerv(GL_CLIP_DEPTH_MODE_EXT, &clipDepthMode);
    EXPECT_GLENUM_EQ(GL_LOWER_LEFT_EXT, clipOrigin);
    EXPECT_GLENUM_EQ(GL_NEGATIVE_ONE_TO_ONE_EXT, clipDepthMode);

    ASSERT_GL_NO_ERROR();

    // Check valid state updates
    glClipControlEXT(GL_LOWER_LEFT_EXT, GL_NEGATIVE_ONE_TO_ONE_EXT);
    glGetIntegerv(GL_CLIP_ORIGIN_EXT, &clipOrigin);
    glGetIntegerv(GL_CLIP_DEPTH_MODE_EXT, &clipDepthMode);
    EXPECT_GLENUM_EQ(GL_LOWER_LEFT_EXT, clipOrigin);
    EXPECT_GLENUM_EQ(GL_NEGATIVE_ONE_TO_ONE_EXT, clipDepthMode);

    glClipControlEXT(GL_LOWER_LEFT_EXT, GL_ZERO_TO_ONE_EXT);
    glGetIntegerv(GL_CLIP_ORIGIN_EXT, &clipOrigin);
    glGetIntegerv(GL_CLIP_DEPTH_MODE_EXT, &clipDepthMode);
    EXPECT_GLENUM_EQ(GL_LOWER_LEFT_EXT, clipOrigin);
    EXPECT_GLENUM_EQ(GL_ZERO_TO_ONE_EXT, clipDepthMode);

    glClipControlEXT(GL_UPPER_LEFT_EXT, GL_NEGATIVE_ONE_TO_ONE_EXT);
    glGetIntegerv(GL_CLIP_ORIGIN_EXT, &clipOrigin);
    glGetIntegerv(GL_CLIP_DEPTH_MODE_EXT, &clipDepthMode);
    EXPECT_GLENUM_EQ(GL_UPPER_LEFT_EXT, clipOrigin);
    EXPECT_GLENUM_EQ(GL_NEGATIVE_ONE_TO_ONE_EXT, clipDepthMode);

    glClipControlEXT(GL_UPPER_LEFT_EXT, GL_ZERO_TO_ONE_EXT);
    glGetIntegerv(GL_CLIP_ORIGIN_EXT, &clipOrigin);
    glGetIntegerv(GL_CLIP_DEPTH_MODE_EXT, &clipDepthMode);
    EXPECT_GLENUM_EQ(GL_UPPER_LEFT_EXT, clipOrigin);
    EXPECT_GLENUM_EQ(GL_ZERO_TO_ONE_EXT, clipDepthMode);

    ASSERT_GL_NO_ERROR();

    // Check invalid state updates
    glClipControlEXT(GL_LOWER_LEFT_EXT, 0);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glClipControlEXT(0, GL_NEGATIVE_ONE_TO_ONE_EXT);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Invalid command does not change the state
    glGetIntegerv(GL_CLIP_ORIGIN_EXT, &clipOrigin);
    glGetIntegerv(GL_CLIP_DEPTH_MODE_EXT, &clipDepthMode);
    EXPECT_GLENUM_EQ(GL_UPPER_LEFT_EXT, clipOrigin);
    EXPECT_GLENUM_EQ(GL_ZERO_TO_ONE_EXT, clipDepthMode);

    ASSERT_GL_NO_ERROR();
}

// Test that clip origin does not affect scissored clears
TEST_P(ClipControlTest, OriginScissorClear)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_clip_control"));

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Blue());

    auto test = [&](std::string name, bool useES3) {
        // Start with lower-left
        glClipControlEXT(GL_LOWER_LEFT_EXT, GL_NEGATIVE_ONE_TO_ONE_EXT);

        // Make a draw call without color writes to sync the state
        glColorMask(false, false, false, false);
        drawQuad(program, essl1_shaders::PositionAttrib(), 0.0);
        glColorMask(true, true, true, true);

        // Clear to red
        glDisable(GL_SCISSOR_TEST);
        if (useES3)
        {
            float color[4] = {1.0, 0.0, 0.0, 1.0};
            glClearBufferfv(GL_COLOR, 0, color);
        }
        else
        {
            glClearColor(1.0, 0.0, 0.0, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // Flip the clip origin
        glClipControlEXT(GL_UPPER_LEFT_EXT, GL_NEGATIVE_ONE_TO_ONE_EXT);

        // Make a draw call without color writes to sync the state
        glColorMask(false, false, false, false);
        drawQuad(program, essl1_shaders::PositionAttrib(), 0.0);
        glColorMask(true, true, true, true);

        // Clear lower half to green
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, w, h / 2);
        if (useES3)
        {
            float color[4] = {0.0, 1.0, 0.0, 1.0};
            glClearBufferfv(GL_COLOR, 0, color);
        }
        else
        {
            glClearColor(0.0, 1.0, 0.0, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(0.5 * w, 0.25 * h, GLColor::green) << name;
        EXPECT_PIXEL_COLOR_EQ(0.5 * w, 0.75 * h, GLColor::red) << name;
    };

    test("Default framebuffer", getClientMajorVersion() > 2);

    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_OES_rgb8_rgba8"));

    GLRenderbuffer rb;
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h);

    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    test("User framebuffer", getClientMajorVersion() > 2);
}

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(ClipControlTest);
