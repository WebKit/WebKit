//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ClipDistanceTest.cpp: Test cases for GL_APPLE_clip_distance/GL_EXT_clip_cull_distance extension.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"
#include "util/test_utils.h"

using namespace angle;

class ClipDistanceTest : public ANGLETest
{
  protected:
    ClipDistanceTest()
    {
        setWindowWidth(16);
        setWindowHeight(16);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
    }
};

// Query max clip distances and enable, disable states of clip distances
TEST_P(ClipDistanceTest, StateQuery)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_APPLE_clip_distance"));

    GLint maxClipDistances = 0;
    glGetIntegerv(GL_MAX_CLIP_DISTANCES_APPLE, &maxClipDistances);

    EXPECT_GL_NO_ERROR();
    EXPECT_GE(maxClipDistances, 8);

    GLboolean enabled = glIsEnabled(GL_CLIP_DISTANCE1_APPLE);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(enabled, GL_FALSE);

    glEnable(GL_CLIP_DISTANCE1_APPLE);
    EXPECT_GL_NO_ERROR();
    glEnable(GL_CLIP_DISTANCE7_APPLE);
    EXPECT_GL_NO_ERROR();
    enabled = glIsEnabled(GL_CLIP_DISTANCE1_APPLE);
    EXPECT_EQ(enabled, GL_TRUE);

    glDisable(GL_CLIP_DISTANCE1_APPLE);
    EXPECT_GL_NO_ERROR();
    enabled = glIsEnabled(GL_CLIP_DISTANCE1_APPLE);
    EXPECT_EQ(enabled, GL_FALSE);

    EXPECT_EQ(glIsEnabled(GL_CLIP_DISTANCE7_APPLE), GL_TRUE);
}

// Write to one gl_ClipDistance element
TEST_P(ClipDistanceTest, OneClipDistance)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_APPLE_clip_distance"));

    constexpr char kVS[] = R"(
#extension GL_APPLE_clip_distance : require

uniform vec4 u_plane;

attribute vec2 a_position;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    gl_ClipDistance[0] = dot(gl_Position, u_plane);
})";

    ANGLE_GL_PROGRAM(programRed, kVS, essl1_shaders::fs::Red());
    glLinkProgram(programRed);
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    glEnable(GL_CLIP_DISTANCE0_APPLE);

    // Clear to blue
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw full screen quad with color red
    glUniform4f(glGetUniformLocation(programRed, "u_plane"), 1, 0, 0, 0.5);
    EXPECT_GL_NO_ERROR();
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    // All pixels on the left of the plane x = -0.5 must be blue
    for (int x = 0; x < getWindowWidth() / 4 - 1; ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::blue);
        }
    }

    // All pixels on the right of the plane x = -0.5 must be red
    for (int x = getWindowWidth() / 4 + 2; x < getWindowWidth(); ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::red);
        }
    }

    // Clear to green
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw full screen quad with color red
    glUniform4f(glGetUniformLocation(programRed, "u_plane"), -1, 0, 0, -0.5);
    EXPECT_GL_NO_ERROR();
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    // All pixels on the left of the plane x = -0.5 must be red
    for (int x = 0; x < getWindowWidth() / 4 - 1; ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::red);
        }
    }

    // All pixels on the right of the plane x = -0.5 must be green
    for (int x = getWindowWidth() / 4 + 2; x < getWindowWidth(); ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::green);
        }
    }

    // Disable GL_CLIP_DISTANCE
    glDisable(GL_CLIP_DISTANCE0_APPLE);
    drawQuad(programRed, "a_position", 0);

    // All pixels must be red
    for (int x = 0; x < getWindowWidth(); ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::red);
        }
    }
}

// Write to 3 clip distances
TEST_P(ClipDistanceTest, ThreeClipDistances)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_APPLE_clip_distance"));

    constexpr char kVS[] = R"(
#extension GL_APPLE_clip_distance : require

uniform vec4 u_plane[3];

attribute vec2 a_position;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    gl_ClipDistance[0] = dot(gl_Position, u_plane[0]);
    gl_ClipDistance[3] = dot(gl_Position, u_plane[1]);
    gl_ClipDistance[7] = dot(gl_Position, u_plane[2]);
})";

    ANGLE_GL_PROGRAM(programRed, kVS, essl1_shaders::fs::Red());
    glLinkProgram(programRed);
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    // Enable 3 clip distances
    glEnable(GL_CLIP_DISTANCE0_APPLE);
    glEnable(GL_CLIP_DISTANCE3_APPLE);
    glEnable(GL_CLIP_DISTANCE7_APPLE);
    ASSERT_GL_NO_ERROR();

    // Clear to blue
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw full screen quad with color red
    // x = -0.5
    glUniform4f(glGetUniformLocation(programRed, "u_plane[0]"), 1, 0, 0, 0.5);
    // x = 0.5
    glUniform4f(glGetUniformLocation(programRed, "u_plane[1]"), -1, 0, 0, 0.5);
    // x + y = 1
    glUniform4f(glGetUniformLocation(programRed, "u_plane[2]"), -1, -1, 0, 1);
    EXPECT_GL_NO_ERROR();
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    // All pixels on the left of the plane x = -0.5 must be blue
    for (int x = 0; x < getWindowWidth() / 4 - 1; ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::blue);
        }
    }

    // All pixels on the right of the plane x = -0.5 must be red, except those in the upper right
    // triangle
    for (int x = getWindowWidth() / 4 + 2; x < getWindowWidth() / 2; ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::red);
        }
    }

    for (int y = 0; y < getWindowHeight(); ++y)
    {
        for (int x = getWindowWidth() / 2; x < getWindowWidth(); ++x)
        {
            if (x < getWindowWidth() * 3 / 2 - y - 1 && x < getWindowWidth() * 3 / 4 - 1)
            {
                // bottom left triangle clipped by x=0.5 plane
                EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::red);
            }
            else if (x > getWindowWidth() * 3 / 2 - y + 1 || x > getWindowWidth() * 3 / 4 + 1)
            {
                // upper right triangle plus right of x=0.5 plane
                EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::blue);
            }
        }
    }

    // Clear to green
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Disable gl_ClipDistance[3]
    glDisable(GL_CLIP_DISTANCE3_APPLE);

    // Draw full screen quad with color red
    EXPECT_GL_NO_ERROR();
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    // All pixels on the left of the plane x = -0.5 must be green
    for (int x = 0; x < getWindowWidth() / 4 - 1; ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::green);
        }
    }

    // All pixels on the right of the plane x = -0.5 must be red, except those in the upper right
    // triangle
    for (int x = getWindowWidth() / 4 + 2; x < getWindowWidth() / 2; ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::red);
        }
    }

    for (int y = 0; y < getWindowHeight(); ++y)
    {
        for (int x = getWindowWidth() / 2; x < getWindowWidth(); ++x)
        {
            if (x < getWindowWidth() * 3 / 2 - y - 1)
            {
                // bottom left triangle
                EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::red);
            }
            else if (x > getWindowWidth() * 3 / 2 - y + 1)
            {
                // upper right triangle
                EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::green);
            }
        }
    }
}

// Redeclare gl_ClipDistance in shader with explitcit size, also use it in a global function
// outside main()
TEST_P(ClipDistanceTest, ThreeClipDistancesRedeclared)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_APPLE_clip_distance"));

    constexpr char kVS[] = R"(
#extension GL_APPLE_clip_distance : require

varying highp float gl_ClipDistance[3];

void computeClipDistances(in vec4 position, in vec4 plane[3])
{
    gl_ClipDistance[0] = dot(position, plane[0]);
    gl_ClipDistance[1] = dot(position, plane[1]);
    gl_ClipDistance[2] = dot(position, plane[2]);
}

uniform vec4 u_plane[3];

attribute vec2 a_position;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    computeClipDistances(gl_Position, u_plane);
})";

    ANGLE_GL_PROGRAM(programRed, kVS, essl1_shaders::fs::Red());
    glLinkProgram(programRed);
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    // Enable 3 clip distances
    glEnable(GL_CLIP_DISTANCE0_APPLE);
    glEnable(GL_CLIP_DISTANCE1_APPLE);
    glEnable(GL_CLIP_DISTANCE2_APPLE);
    ASSERT_GL_NO_ERROR();

    // Clear to blue
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw full screen quad with color red
    // x = -0.5
    glUniform4f(glGetUniformLocation(programRed, "u_plane[0]"), 1, 0, 0, 0.5);
    // x = 0.5
    glUniform4f(glGetUniformLocation(programRed, "u_plane[1]"), -1, 0, 0, 0.5);
    // x + y = 1
    glUniform4f(glGetUniformLocation(programRed, "u_plane[2]"), -1, -1, 0, 1);
    EXPECT_GL_NO_ERROR();
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    // All pixels on the left of the plane x = -0.5 must be blue
    for (int x = 0; x < getWindowWidth() / 4 - 1; ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::blue);
        }
    }

    // All pixels on the right of the plane x = -0.5 must be red, except those in the upper right
    // triangle
    for (int x = getWindowWidth() / 4 + 2; x < getWindowWidth() / 2; ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::red);
        }
    }

    for (int y = 0; y < getWindowHeight(); ++y)
    {
        for (int x = getWindowWidth() / 2; x < getWindowWidth(); ++x)
        {
            if (x < getWindowWidth() * 3 / 2 - y - 1 && x < getWindowWidth() * 3 / 4 - 1)
            {
                // bottom left triangle clipped by x=0.5 plane
                EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::red);
            }
            else if (x > getWindowWidth() * 3 / 2 - y + 1 || x > getWindowWidth() * 3 / 4 + 1)
            {
                // upper right triangle plus right of x=0.5 plane
                EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::blue);
            }
        }
    }
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(ClipDistanceTest);
