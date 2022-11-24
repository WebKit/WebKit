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

class ClipDistanceTest : public ANGLETest<>
{
  protected:
    ClipDistanceTest()
    {
        setWindowWidth(64);
        setWindowHeight(64);
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
    GLuint x      = 0;
    GLuint y      = 0;
    GLuint width  = getWindowWidth() / 4 - 1;
    GLuint height = getWindowHeight();
    EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::blue);

    // All pixels on the right of the plane x = -0.5 must be red
    x      = getWindowWidth() / 4 + 2;
    y      = 0;
    width  = getWindowWidth() - x;
    height = getWindowHeight();
    EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);

    // Clear to green
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw full screen quad with color red
    glUniform4f(glGetUniformLocation(programRed, "u_plane"), -1, 0, 0, -0.5);
    EXPECT_GL_NO_ERROR();
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    // All pixels on the left of the plane x = -0.5 must be red
    x      = 0;
    y      = 0;
    width  = getWindowWidth() / 4 - 1;
    height = getWindowHeight();
    EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);

    // All pixels on the right of the plane x = -0.5 must be green
    x      = getWindowWidth() / 4 + 2;
    y      = 0;
    width  = getWindowWidth() - x;
    height = getWindowHeight();
    EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::green);

    // Disable GL_CLIP_DISTANCE
    glDisable(GL_CLIP_DISTANCE0_APPLE);
    drawQuad(programRed, "a_position", 0);

    // All pixels must be red
    x      = 0;
    y      = 0;
    width  = getWindowWidth();
    height = getWindowHeight();
    EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);
}

// Write to each gl_ClipDistance element
TEST_P(ClipDistanceTest, EachClipDistance)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_APPLE_clip_distance"));

    for (size_t i = 0; i < 8; i++)
    {
        std::stringstream vertexShaderStr;
        vertexShaderStr << "#extension GL_APPLE_clip_distance : require\n"
                        << "uniform vec4 u_plane;"
                        << "attribute vec2 a_position;"
                        << "void main()"
                        << "{"
                        << "    gl_Position = vec4(a_position, 0.0, 1.0);"
                        << "    gl_ClipDistance[" << i << "] = dot(gl_Position, u_plane);"
                        << "}";

        ANGLE_GL_PROGRAM(programRed, vertexShaderStr.str().c_str(), essl1_shaders::fs::Red());
        glLinkProgram(programRed);
        glUseProgram(programRed);
        ASSERT_GL_NO_ERROR();

        // Enable the current clip distance, disable all others.
        for (size_t j = 0; j < 8; j++)
        {
            if (j == i)
                glEnable(GL_CLIP_DISTANCE0_APPLE + j);
            else
                glDisable(GL_CLIP_DISTANCE0_APPLE + j);
        }

        // Clear to blue
        glClearColor(0, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw full screen quad with color red
        glUniform4f(glGetUniformLocation(programRed, "u_plane"), 1, 0, 0, 0.5);
        EXPECT_GL_NO_ERROR();
        drawQuad(programRed, "a_position", 0);
        EXPECT_GL_NO_ERROR();

        // All pixels on the left of the plane x = -0.5 must be blue
        GLuint x      = 0;
        GLuint y      = 0;
        GLuint width  = getWindowWidth() / 4 - 1;
        GLuint height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::blue);

        // All pixels on the right of the plane x = -0.5 must be red
        x      = getWindowWidth() / 4 + 2;
        y      = 0;
        width  = getWindowWidth() - x;
        height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);

        // Clear to green
        glClearColor(0, 1, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw full screen quad with color red
        glUniform4f(glGetUniformLocation(programRed, "u_plane"), -1, 0, 0, -0.5);
        EXPECT_GL_NO_ERROR();
        drawQuad(programRed, "a_position", 0);
        EXPECT_GL_NO_ERROR();

        // All pixels on the left of the plane x = -0.5 must be red
        x      = 0;
        y      = 0;
        width  = getWindowWidth() / 4 - 1;
        height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);

        // All pixels on the right of the plane x = -0.5 must be green
        x      = getWindowWidth() / 4 + 2;
        y      = 0;
        width  = getWindowWidth() - x;
        height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::green);

        // Disable GL_CLIP_DISTANCE
        glDisable(GL_CLIP_DISTANCE0_APPLE + i);
        drawQuad(programRed, "a_position", 0);

        // All pixels must be red
        x      = 0;
        y      = 0;
        width  = getWindowWidth();
        height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);
    }
}

// Use 8 clip distances to draw an octagon
TEST_P(ClipDistanceTest, Octagon)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_APPLE_clip_distance"));

    constexpr char kVS[] = R"(
#extension GL_APPLE_clip_distance : require

attribute vec2 a_position;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    gl_ClipDistance[0] = dot(gl_Position, vec4( 1,  0, 0, 0.5));
    gl_ClipDistance[1] = dot(gl_Position, vec4(-1,  0, 0, 0.5));
    gl_ClipDistance[2] = dot(gl_Position, vec4( 0,  1, 0, 0.5));
    gl_ClipDistance[3] = dot(gl_Position, vec4( 0, -1, 0, 0.5));
    gl_ClipDistance[4] = dot(gl_Position, vec4( 1,  1, 0, 0.70710678));
    gl_ClipDistance[5] = dot(gl_Position, vec4( 1, -1, 0, 0.70710678));
    gl_ClipDistance[6] = dot(gl_Position, vec4(-1,  1, 0, 0.70710678));
    gl_ClipDistance[7] = dot(gl_Position, vec4(-1, -1, 0, 0.70710678));
})";

    ANGLE_GL_PROGRAM(programRed, kVS, essl1_shaders::fs::Red());
    glLinkProgram(programRed);
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    glEnable(GL_CLIP_DISTANCE0_APPLE);
    glEnable(GL_CLIP_DISTANCE1_APPLE);
    glEnable(GL_CLIP_DISTANCE2_APPLE);
    glEnable(GL_CLIP_DISTANCE3_APPLE);
    glEnable(GL_CLIP_DISTANCE4_APPLE);
    glEnable(GL_CLIP_DISTANCE5_APPLE);
    glEnable(GL_CLIP_DISTANCE6_APPLE);
    glEnable(GL_CLIP_DISTANCE7_APPLE);

    // Clear to blue
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw full screen quad with color red
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    // Top edge
    EXPECT_PIXEL_COLOR_EQ(32, 56, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(32, 40, GLColor::red);

    // Top-right edge
    EXPECT_PIXEL_COLOR_EQ(48, 48, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(40, 40, GLColor::red);

    // Right edge
    EXPECT_PIXEL_COLOR_EQ(56, 32, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(40, 32, GLColor::red);

    // Bottom-right edge
    EXPECT_PIXEL_COLOR_EQ(48, 16, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(40, 24, GLColor::red);

    // Bottom edge
    EXPECT_PIXEL_COLOR_EQ(32, 8, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(32, 24, GLColor::red);

    // Bottom-left edge
    EXPECT_PIXEL_COLOR_EQ(16, 16, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(24, 24, GLColor::red);

    // Left edge
    EXPECT_PIXEL_COLOR_EQ(8, 32, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(24, 32, GLColor::red);

    // Top-left edge
    EXPECT_PIXEL_COLOR_EQ(16, 48, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(24, 40, GLColor::red);
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

    {
        // All pixels on the left of the plane x = -0.5 must be blue
        GLuint x      = 0;
        GLuint y      = 0;
        GLuint width  = getWindowWidth() / 4 - 1;
        GLuint height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::blue);

        // All pixels from the plane x = -0.5 to the plane x = 0 must be red
        x      = getWindowWidth() / 4 + 2;
        y      = 0;
        width  = getWindowWidth() / 2 - x;
        height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);
    }

    {
        // Check pixels to the right of the plane x = 0
        std::vector<GLColor> actualColors(getWindowWidth() * getWindowHeight());
        glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                     actualColors.data());
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            for (int x = getWindowWidth() / 2; x < getWindowWidth(); ++x)
            {
                const int currentPosition = y * getWindowHeight() + x;

                if (x < getWindowWidth() * 3 / 2 - y - 1 && x < getWindowWidth() * 3 / 4 - 1)
                {
                    // Bottom of the plane x + y = 1 clipped by x = 0.5 plane
                    EXPECT_EQ(GLColor::red, actualColors[currentPosition]);
                }
                else if (x > getWindowWidth() * 3 / 2 - y + 1 || x > getWindowWidth() * 3 / 4 + 1)
                {
                    // Top of the plane x + y = 1 plus right of x = 0.5 plane
                    EXPECT_EQ(GLColor::blue, actualColors[currentPosition]);
                }
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

    {
        // All pixels on the left of the plane x = -0.5 must be green
        GLuint x      = 0;
        GLuint y      = 0;
        GLuint width  = getWindowWidth() / 4 - 1;
        GLuint height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::green);

        // All pixels from the plane x = -0.5 to the plane x = 0 must be red
        x      = getWindowWidth() / 4 + 2;
        y      = 0;
        width  = getWindowWidth() / 2 - x;
        height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);
    }

    // Check pixels to the right of the plane x = 0
    std::vector<GLColor> actualColors(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actualColors.data());
    for (int y = 0; y < getWindowHeight(); ++y)
    {
        for (int x = getWindowWidth() / 2; x < getWindowWidth(); ++x)
        {
            const int currentPosition = y * getWindowHeight() + x;

            if (x < getWindowWidth() * 3 / 2 - y - 1)
            {
                // Bottom of the plane x + y = 1
                EXPECT_EQ(GLColor::red, actualColors[currentPosition]);
            }
            else if (x > getWindowWidth() * 3 / 2 - y + 1)
            {
                // Top of the plane x + y = 1
                EXPECT_EQ(GLColor::green, actualColors[currentPosition]);
            }
        }
    }
}

// Redeclare gl_ClipDistance in shader with explicit size, also use it in a global function
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

    {
        // All pixels on the left of the plane x = -0.5 must be blue
        GLuint x      = 0;
        GLuint y      = 0;
        GLuint width  = getWindowWidth() / 4 - 1;
        GLuint height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::blue);

        // All pixels from the plane x = -0.5 to the plane x = 0 must be red
        x      = getWindowWidth() / 4 + 2;
        y      = 0;
        width  = getWindowWidth() / 2 - x;
        height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);
    }

    // Check pixels to the right of the plane x = 0
    std::vector<GLColor> actualColors(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actualColors.data());
    for (int y = 0; y < getWindowHeight(); ++y)
    {
        for (int x = getWindowWidth() / 2; x < getWindowWidth(); ++x)
        {
            const int currentPosition = y * getWindowHeight() + x;

            if (x < getWindowWidth() * 3 / 2 - y - 1 && x < getWindowWidth() * 3 / 4 - 1)
            {
                // Bottom of the plane x + y = 1 clipped by x = 0.5 plane
                EXPECT_EQ(GLColor::red, actualColors[currentPosition]);
            }
            else if (x > getWindowWidth() * 3 / 2 - y + 1 || x > getWindowWidth() * 3 / 4 + 1)
            {
                // Top of the plane x + y = 1 plus right of x = 0.5 plane
                EXPECT_EQ(GLColor::blue, actualColors[currentPosition]);
            }
        }
    }
}

class ClipCullDistanceTest : public ClipDistanceTest
{};

// Query max clip distances and enable, disable states of clip distances
TEST_P(ClipCullDistanceTest, StateQuery)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance"));

    GLint maxClipDistances = 0;
    glGetIntegerv(GL_MAX_CLIP_DISTANCES_EXT, &maxClipDistances);

    EXPECT_GL_NO_ERROR();
    EXPECT_GE(maxClipDistances, 8);

    GLint maxCullDistances = 0;
    glGetIntegerv(GL_MAX_CULL_DISTANCES_EXT, &maxCullDistances);

    EXPECT_GL_NO_ERROR();
    EXPECT_GE(maxCullDistances, 8);

    GLint maxCombinedClipAndCullDistances = 0;
    glGetIntegerv(GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES_EXT, &maxCombinedClipAndCullDistances);

    EXPECT_GL_NO_ERROR();
    EXPECT_GE(maxCombinedClipAndCullDistances, 8);

    GLboolean enabled = glIsEnabled(GL_CLIP_DISTANCE1_EXT);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(enabled, GL_FALSE);

    glEnable(GL_CLIP_DISTANCE1_EXT);
    EXPECT_GL_NO_ERROR();
    glEnable(GL_CLIP_DISTANCE7_EXT);
    EXPECT_GL_NO_ERROR();
    enabled = glIsEnabled(GL_CLIP_DISTANCE1_EXT);
    EXPECT_EQ(enabled, GL_TRUE);

    glDisable(GL_CLIP_DISTANCE1_EXT);
    EXPECT_GL_NO_ERROR();
    enabled = glIsEnabled(GL_CLIP_DISTANCE1_EXT);
    EXPECT_EQ(enabled, GL_FALSE);

    EXPECT_EQ(glIsEnabled(GL_CLIP_DISTANCE7_EXT), GL_TRUE);
}

// Check that the validation for EXT_clip_cull_distance extension is correct
// If gl_ClipDistance or gl_CullDistance is redeclared in some shader stages, the array size of the
// redeclared variables should match
TEST_P(ClipCullDistanceTest, SizeCheck)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance"));

    // Validate array size match of redeclared ClipDistance built-in
    constexpr char kVSErrorClipDistance[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require

out highp float gl_ClipDistance[1];

uniform vec4 u_plane;

in vec2 a_position;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    gl_ClipDistance[0] = dot(gl_Position, u_plane);

    gl_CullDistance[0] = dot(gl_Position, u_plane);
    gl_CullDistance[1] = dot(gl_Position, u_plane);
})";

    constexpr char kFSErrorClipDistance[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require
in highp float gl_ClipDistance[2];
precision highp float;
out vec4 my_FragColor;
void main()
{
    my_FragColor = vec4(gl_ClipDistance[0], gl_ClipDistance[1], gl_CullDistance[0], 1.0f);
})";

    GLProgram programClipDistance;
    programClipDistance.makeRaster(kVSErrorClipDistance, kFSErrorClipDistance);
    EXPECT_GL_FALSE(programClipDistance.valid());

    // Validate array size match of redeclared CullDistance built-in
    constexpr char kVSErrorCullDistance[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require

out highp float gl_CullDistance[1];

uniform vec4 u_plane;

in vec2 a_position;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    gl_CullDistance[0] = dot(gl_Position, u_plane);

    gl_ClipDistance[0] = dot(gl_Position, u_plane);
    gl_ClipDistance[1] = dot(gl_Position, u_plane);
})";

    constexpr char kFSErrorCullDistance[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require
in highp float gl_CullDistance[2];
precision highp float;
out vec4 my_FragColor;
void main()
{
    my_FragColor = vec4(gl_CullDistance[0], gl_CullDistance[1], gl_ClipDistance[0], 1.0f);
})";

    GLProgram programCullDistance;
    programCullDistance.makeRaster(kVSErrorCullDistance, kFSErrorCullDistance);
    EXPECT_GL_FALSE(programCullDistance.valid());
}

// Check the D3D11-specific limitation for EXT_clip_cull_distance extension.
// If both gl_ClipDistance and gl_CullDistance are used, their sizes must not be greater than 4.
TEST_P(ClipCullDistanceTest, SizeCheckD3D11)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance"));

    constexpr char kVSErrorClipDistanceExplicit[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require
out highp float gl_ClipDistance[5];
out highp float gl_CullDistance[3];
in vec2 a_position;
void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    gl_ClipDistance[4] = dot(gl_Position, vec4(1.0, 2.0, 3.0, 4.0));
    gl_CullDistance[2] = dot(gl_Position, vec4(5.0, 6.0, 7.0, 8.0));
})";

    constexpr char kVSErrorClipDistanceImplicit[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require
in vec2 a_position;
void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    gl_ClipDistance[4] = dot(gl_Position, vec4(1.0, 2.0, 3.0, 4.0));
    gl_CullDistance[2] = dot(gl_Position, vec4(5.0, 6.0, 7.0, 8.0));
})";

    constexpr char kVSErrorCullDistanceExplicit[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require
out highp float gl_ClipDistance[3];
out highp float gl_CullDistance[5];
in vec2 a_position;
void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    gl_ClipDistance[2] = dot(gl_Position, vec4(1.0, 2.0, 3.0, 4.0));
    gl_CullDistance[4] = dot(gl_Position, vec4(5.0, 6.0, 7.0, 8.0));
})";

    constexpr char kVSErrorCullDistanceImplicit[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require
in vec2 a_position;
void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    gl_ClipDistance[2] = dot(gl_Position, vec4(1.0, 2.0, 3.0, 4.0));
    gl_CullDistance[4] = dot(gl_Position, vec4(5.0, 6.0, 7.0, 8.0));
})";

    for (auto vs : {kVSErrorClipDistanceExplicit, kVSErrorClipDistanceImplicit,
                    kVSErrorCullDistanceExplicit, kVSErrorCullDistanceImplicit})
    {
        GLProgram prg;
        prg.makeRaster(vs, essl3_shaders::fs::Red());
        EXPECT_EQ(prg.valid(), !IsD3D11());
    }
}

// Write to one gl_ClipDistance element
TEST_P(ClipCullDistanceTest, OneClipDistance)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance"));

    constexpr char kVS[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require

uniform vec4 u_plane;

in vec2 a_position;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    gl_ClipDistance[0] = dot(gl_Position, u_plane);
})";

    ANGLE_GL_PROGRAM(programRed, kVS, essl3_shaders::fs::Red());
    glLinkProgram(programRed);
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    glEnable(GL_CLIP_DISTANCE0_EXT);

    // Clear to blue
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw full screen quad with color red
    glUniform4f(glGetUniformLocation(programRed, "u_plane"), 1, 0, 0, 0.5);
    EXPECT_GL_NO_ERROR();
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    // All pixels on the left of the plane x = -0.5 must be blue
    GLuint x      = 0;
    GLuint y      = 0;
    GLuint width  = getWindowWidth() / 4 - 1;
    GLuint height = getWindowHeight();
    EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::blue);

    // All pixels on the right of the plane x = -0.5 must be red
    x      = getWindowWidth() / 4 + 2;
    y      = 0;
    width  = getWindowWidth() - x;
    height = getWindowHeight();
    EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);

    // Clear to green
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw full screen quad with color red
    glUniform4f(glGetUniformLocation(programRed, "u_plane"), -1, 0, 0, -0.5);
    EXPECT_GL_NO_ERROR();
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    // All pixels on the left of the plane x = -0.5 must be red
    x      = 0;
    y      = 0;
    width  = getWindowWidth() / 4 - 1;
    height = getWindowHeight();
    EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);

    // All pixels on the right of the plane x = -0.5 must be green
    x      = getWindowWidth() / 4 + 2;
    y      = 0;
    width  = getWindowWidth() - x;
    height = getWindowHeight();
    EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::green);

    // Disable GL_CLIP_DISTANCE
    glDisable(GL_CLIP_DISTANCE0_EXT);
    drawQuad(programRed, "a_position", 0);

    // All pixels must be red
    x      = 0;
    y      = 0;
    width  = getWindowWidth();
    height = getWindowHeight();
    EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);
}

// Write to each gl_ClipDistance element
TEST_P(ClipCullDistanceTest, EachClipDistance)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance"));

    for (size_t i = 0; i < 8; i++)
    {
        std::stringstream vertexShaderStr;
        vertexShaderStr << "#version 300 es\n"
                        << "#extension GL_EXT_clip_cull_distance : require\n"
                        << "uniform vec4 u_plane;"
                        << "in vec2 a_position;"
                        << "void main()"
                        << "{"
                        << "    gl_Position = vec4(a_position, 0.0, 1.0);"
                        << "    gl_ClipDistance[" << i << "] = dot(gl_Position, u_plane);"
                        << "}";

        ANGLE_GL_PROGRAM(programRed, vertexShaderStr.str().c_str(), essl3_shaders::fs::Red());
        glLinkProgram(programRed);
        glUseProgram(programRed);
        ASSERT_GL_NO_ERROR();

        // Enable the current clip distance, disable all others.
        for (size_t j = 0; j < 8; j++)
        {
            if (j == i)
                glEnable(GL_CLIP_DISTANCE0_EXT + j);
            else
                glDisable(GL_CLIP_DISTANCE0_EXT + j);
        }

        // Clear to blue
        glClearColor(0, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw full screen quad with color red
        glUniform4f(glGetUniformLocation(programRed, "u_plane"), 1, 0, 0, 0.5);
        EXPECT_GL_NO_ERROR();
        drawQuad(programRed, "a_position", 0);
        EXPECT_GL_NO_ERROR();

        // All pixels on the left of the plane x = -0.5 must be blue
        GLuint x      = 0;
        GLuint y      = 0;
        GLuint width  = getWindowWidth() / 4 - 1;
        GLuint height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::blue);

        // All pixels on the right of the plane x = -0.5 must be red
        x      = getWindowWidth() / 4 + 2;
        y      = 0;
        width  = getWindowWidth() - x;
        height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);

        // Clear to green
        glClearColor(0, 1, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw full screen quad with color red
        glUniform4f(glGetUniformLocation(programRed, "u_plane"), -1, 0, 0, -0.5);
        EXPECT_GL_NO_ERROR();
        drawQuad(programRed, "a_position", 0);
        EXPECT_GL_NO_ERROR();

        // All pixels on the left of the plane x = -0.5 must be red
        x      = 0;
        y      = 0;
        width  = getWindowWidth() / 4 - 1;
        height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);

        // All pixels on the right of the plane x = -0.5 must be green
        x      = getWindowWidth() / 4 + 2;
        y      = 0;
        width  = getWindowWidth() - x;
        height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::green);

        // Disable GL_CLIP_DISTANCE
        glDisable(GL_CLIP_DISTANCE0_EXT + i);
        drawQuad(programRed, "a_position", 0);

        // All pixels must be red
        x      = 0;
        y      = 0;
        width  = getWindowWidth();
        height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);
    }
}

// Use 8 clip distances to draw an octagon
TEST_P(ClipCullDistanceTest, Octagon)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance"));

    constexpr char kVS[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require

in vec2 a_position;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    gl_ClipDistance[0] = dot(gl_Position, vec4( 1,  0, 0, 0.5));
    gl_ClipDistance[1] = dot(gl_Position, vec4(-1,  0, 0, 0.5));
    gl_ClipDistance[2] = dot(gl_Position, vec4( 0,  1, 0, 0.5));
    gl_ClipDistance[3] = dot(gl_Position, vec4( 0, -1, 0, 0.5));
    gl_ClipDistance[4] = dot(gl_Position, vec4( 1,  1, 0, 0.70710678));
    gl_ClipDistance[5] = dot(gl_Position, vec4( 1, -1, 0, 0.70710678));
    gl_ClipDistance[6] = dot(gl_Position, vec4(-1,  1, 0, 0.70710678));
    gl_ClipDistance[7] = dot(gl_Position, vec4(-1, -1, 0, 0.70710678));
})";

    ANGLE_GL_PROGRAM(programRed, kVS, essl3_shaders::fs::Red());
    glLinkProgram(programRed);
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    glEnable(GL_CLIP_DISTANCE0_EXT);
    glEnable(GL_CLIP_DISTANCE1_EXT);
    glEnable(GL_CLIP_DISTANCE2_EXT);
    glEnable(GL_CLIP_DISTANCE3_EXT);
    glEnable(GL_CLIP_DISTANCE4_EXT);
    glEnable(GL_CLIP_DISTANCE5_EXT);
    glEnable(GL_CLIP_DISTANCE6_EXT);
    glEnable(GL_CLIP_DISTANCE7_EXT);

    // Clear to blue
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw full screen quad with color red
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    // Top edge
    EXPECT_PIXEL_COLOR_EQ(32, 56, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(32, 40, GLColor::red);

    // Top-right edge
    EXPECT_PIXEL_COLOR_EQ(48, 48, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(40, 40, GLColor::red);

    // Right edge
    EXPECT_PIXEL_COLOR_EQ(56, 32, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(40, 32, GLColor::red);

    // Bottom-right edge
    EXPECT_PIXEL_COLOR_EQ(48, 16, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(40, 24, GLColor::red);

    // Bottom edge
    EXPECT_PIXEL_COLOR_EQ(32, 8, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(32, 24, GLColor::red);

    // Bottom-left edge
    EXPECT_PIXEL_COLOR_EQ(16, 16, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(24, 24, GLColor::red);

    // Left edge
    EXPECT_PIXEL_COLOR_EQ(8, 32, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(24, 32, GLColor::red);

    // Top-left edge
    EXPECT_PIXEL_COLOR_EQ(16, 48, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(24, 40, GLColor::red);
}

// Write to 3 clip distances
TEST_P(ClipCullDistanceTest, ThreeClipDistances)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance"));

    constexpr char kVS[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require

uniform vec4 u_plane[3];

in vec2 a_position;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    gl_ClipDistance[0] = dot(gl_Position, u_plane[0]);
    gl_ClipDistance[3] = dot(gl_Position, u_plane[1]);
    gl_ClipDistance[7] = dot(gl_Position, u_plane[2]);
})";

    ANGLE_GL_PROGRAM(programRed, kVS, essl3_shaders::fs::Red());
    glLinkProgram(programRed);
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    // Enable 3 clip distances
    glEnable(GL_CLIP_DISTANCE0_EXT);
    glEnable(GL_CLIP_DISTANCE3_EXT);
    glEnable(GL_CLIP_DISTANCE7_EXT);
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

    {
        // All pixels on the left of the plane x = -0.5 must be blue
        GLuint x      = 0;
        GLuint y      = 0;
        GLuint width  = getWindowWidth() / 4 - 1;
        GLuint height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::blue);

        // All pixels from the plane x = -0.5 to the plane x = 0 must be red
        x      = getWindowWidth() / 4 + 2;
        y      = 0;
        width  = getWindowWidth() / 2 - x;
        height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);
    }

    {
        // Check pixels to the right of the plane x = 0
        std::vector<GLColor> actualColors(getWindowWidth() * getWindowHeight());
        glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                     actualColors.data());
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            for (int x = getWindowWidth() / 2; x < getWindowWidth(); ++x)
            {
                const int currentPosition = y * getWindowHeight() + x;

                if (x < getWindowWidth() * 3 / 2 - y - 1 && x < getWindowWidth() * 3 / 4 - 1)
                {
                    // Bottom of the plane x + y = 1 clipped by x = 0.5 plane
                    EXPECT_EQ(GLColor::red, actualColors[currentPosition]);
                }
                else if (x > getWindowWidth() * 3 / 2 - y + 1 || x > getWindowWidth() * 3 / 4 + 1)
                {
                    // Top of the plane x + y = 1 plus right of x = 0.5 plane
                    EXPECT_EQ(GLColor::blue, actualColors[currentPosition]);
                }
            }
        }
    }

    // Clear to green
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Disable gl_ClipDistance[3]
    glDisable(GL_CLIP_DISTANCE3_EXT);

    // Draw full screen quad with color red
    EXPECT_GL_NO_ERROR();
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    {
        // All pixels on the left of the plane x = -0.5 must be green
        GLuint x      = 0;
        GLuint y      = 0;
        GLuint width  = getWindowWidth() / 4 - 1;
        GLuint height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::green);

        // All pixels from the plane x = -0.5 to the plane x = 0 must be red
        x      = getWindowWidth() / 4 + 2;
        y      = 0;
        width  = getWindowWidth() / 2 - x;
        height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);
    }

    // Check pixels to the right of the plane x = 0
    std::vector<GLColor> actualColors(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actualColors.data());
    for (int y = 0; y < getWindowHeight(); ++y)
    {
        for (int x = getWindowWidth() / 2; x < getWindowWidth(); ++x)
        {
            const int currentPosition = y * getWindowHeight() + x;

            if (x < getWindowWidth() * 3 / 2 - y - 1)
            {
                // Bottom of the plane x + y = 1
                EXPECT_EQ(GLColor::red, actualColors[currentPosition]);
            }
            else if (x > getWindowWidth() * 3 / 2 - y + 1)
            {
                // Top of the plane x + y = 1
                EXPECT_EQ(GLColor::green, actualColors[currentPosition]);
            }
        }
    }
}

// Redeclare gl_ClipDistance in shader with explicit size, also use it in a global function
// outside main()
TEST_P(ClipCullDistanceTest, ThreeClipDistancesRedeclared)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance"));

    constexpr char kVS[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require

out highp float gl_ClipDistance[3];

void computeClipDistances(in vec4 position, in vec4 plane[3])
{
    gl_ClipDistance[0] = dot(position, plane[0]);
    gl_ClipDistance[1] = dot(position, plane[1]);
    gl_ClipDistance[2] = dot(position, plane[2]);
}

uniform vec4 u_plane[3];

in vec2 a_position;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    computeClipDistances(gl_Position, u_plane);
})";

    ANGLE_GL_PROGRAM(programRed, kVS, essl3_shaders::fs::Red());
    glLinkProgram(programRed);
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    // Enable 3 clip distances
    glEnable(GL_CLIP_DISTANCE0_EXT);
    glEnable(GL_CLIP_DISTANCE1_EXT);
    glEnable(GL_CLIP_DISTANCE2_EXT);
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

    {
        // All pixels on the left of the plane x = -0.5 must be blue
        GLuint x      = 0;
        GLuint y      = 0;
        GLuint width  = getWindowWidth() / 4 - 1;
        GLuint height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::blue);

        // All pixels from the plane x = -0.5 to the plane x = 0 must be red
        x      = getWindowWidth() / 4 + 2;
        y      = 0;
        width  = getWindowWidth() / 2 - x;
        height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);
    }

    // Check pixels to the right of the plane x = 0
    std::vector<GLColor> actualColors(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actualColors.data());
    for (int y = 0; y < getWindowHeight(); ++y)
    {
        for (int x = getWindowWidth() / 2; x < getWindowWidth(); ++x)
        {
            const int currentPosition = y * getWindowHeight() + x;

            if (x < getWindowWidth() * 3 / 2 - y - 1 && x < getWindowWidth() * 3 / 4 - 1)
            {
                // Bottom of the plane x + y = 1 clipped by x = 0.5 plane
                EXPECT_EQ(GLColor::red, actualColors[currentPosition]);
            }
            else if (x > getWindowWidth() * 3 / 2 - y + 1 || x > getWindowWidth() * 3 / 4 + 1)
            {
                // Top of the plane x + y = 1 plus right of x = 0.5 plane
                EXPECT_EQ(GLColor::blue, actualColors[currentPosition]);
            }
        }
    }
}

// Read clip distance varyings in fragment shaders
TEST_P(ClipCullDistanceTest, ClipInterpolation)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance"));

    constexpr char kVS[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require
in vec2 a_position;
void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    gl_ClipDistance[0] = dot(gl_Position, vec4( 1,  0, 0, 0.5));
    gl_ClipDistance[1] = dot(gl_Position, vec4(-1,  0, 0, 0.5));
    gl_ClipDistance[2] = dot(gl_Position, vec4( 0,  1, 0, 0.5));
    gl_ClipDistance[3] = dot(gl_Position, vec4( 0, -1, 0, 0.5));
    gl_ClipDistance[4] = gl_ClipDistance[0];
    gl_ClipDistance[5] = gl_ClipDistance[1];
    gl_ClipDistance[6] = gl_ClipDistance[2];
    gl_ClipDistance[7] = gl_ClipDistance[3];
})";

    constexpr char kFS[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require
precision highp float;
out vec4 my_FragColor;
void main()
{
    float r = gl_ClipDistance[0] + gl_ClipDistance[1];
    float g = gl_ClipDistance[2] + gl_ClipDistance[3];
    float b = gl_ClipDistance[4] + gl_ClipDistance[5];
    float a = gl_ClipDistance[6] + gl_ClipDistance[7];
    my_FragColor = vec4(r, g, b, a) * 0.5;
})";

    ANGLE_GL_PROGRAM(programRed, kVS, kFS);
    glLinkProgram(programRed);
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    glEnable(GL_CLIP_DISTANCE0_EXT);
    glEnable(GL_CLIP_DISTANCE1_EXT);
    glEnable(GL_CLIP_DISTANCE2_EXT);
    glEnable(GL_CLIP_DISTANCE3_EXT);
    glEnable(GL_CLIP_DISTANCE4_EXT);
    glEnable(GL_CLIP_DISTANCE5_EXT);
    glEnable(GL_CLIP_DISTANCE6_EXT);
    glEnable(GL_CLIP_DISTANCE7_EXT);

    // Clear to blue
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw full screen quad with color red
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    std::vector<GLColor> actualColors(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actualColors.data());
    for (int x = 0; x < getWindowWidth(); x++)
    {
        for (int y = 0; y < getWindowHeight(); y++)
        {
            const int currentPosition = y * getWindowHeight() + x;

            if (x >= getWindowWidth() / 4 && x < getWindowWidth() * 3 / 4 &&
                y >= getWindowHeight() / 4 && y < getWindowHeight() * 3 / 4)
            {
                EXPECT_COLOR_NEAR(GLColor(127, 127, 127, 127), actualColors[currentPosition], 1);
            }
            else
            {
                EXPECT_EQ(GLColor::blue, actualColors[currentPosition]);
            }
        }
    }
}

// Write to one gl_CullDistance element
TEST_P(ClipCullDistanceTest, OneCullDistance)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance"));

    constexpr char kVS[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require

uniform vec4 u_plane;

in vec2 a_position;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    gl_CullDistance[0] = dot(gl_Position, u_plane);
})";

    ANGLE_GL_PROGRAM(programRed, kVS, essl3_shaders::fs::Red());
    glLinkProgram(programRed);
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    // Clear to blue
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw full screen quad with color red
    glUniform4f(glGetUniformLocation(programRed, "u_plane"), 1, 0, 0, 0.5);
    EXPECT_GL_NO_ERROR();
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    {
        // All pixels must be red
        GLuint x      = 0;
        GLuint y      = 0;
        GLuint width  = getWindowWidth();
        GLuint height = getWindowHeight();
        EXPECT_PIXEL_RECT_EQ(x, y, width, height, GLColor::red);
    }

    // Clear to green
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw full screen quad with color red
    glUniform4f(glGetUniformLocation(programRed, "u_plane"), 1, 1, 0, 0);
    EXPECT_GL_NO_ERROR();
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    // All pixels on the plane y >= -x must be red
    std::vector<GLColor> actualColors(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actualColors.data());
    for (int x = 0; x < getWindowWidth(); ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            const int currentPosition = y * getWindowHeight() + x;

            if ((x + y) >= 0)
            {
                EXPECT_EQ(GLColor::red, actualColors[currentPosition]);
            }
            else
            {
                EXPECT_EQ(GLColor::green, actualColors[currentPosition]);
            }
        }
    }
}

// Read cull distance varyings in fragment shaders
TEST_P(ClipCullDistanceTest, CullInterpolation)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance"));

    constexpr char kVS[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require
in vec2 a_position;
void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    gl_CullDistance[0] = dot(gl_Position, vec4( 1,  0, 0, 1));
    gl_CullDistance[1] = dot(gl_Position, vec4(-1,  0, 0, 1));
    gl_CullDistance[2] = dot(gl_Position, vec4( 0,  1, 0, 1));
    gl_CullDistance[3] = dot(gl_Position, vec4( 0, -1, 0, 1));
    gl_CullDistance[4] = gl_CullDistance[0];
    gl_CullDistance[5] = gl_CullDistance[1];
    gl_CullDistance[6] = gl_CullDistance[2];
    gl_CullDistance[7] = gl_CullDistance[3];
})";

    constexpr char kFS[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require
precision highp float;
out vec4 my_FragColor;
void main()
{
    float r = gl_CullDistance[0] + gl_CullDistance[1];
    float g = gl_CullDistance[2] + gl_CullDistance[3];
    float b = gl_CullDistance[4] + gl_CullDistance[5];
    float a = gl_CullDistance[6] + gl_CullDistance[7];
    my_FragColor = vec4(r, g, b, a) * 0.25;
})";

    ANGLE_GL_PROGRAM(programRed, kVS, kFS);
    glLinkProgram(programRed);
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    // Clear to blue
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw full screen quad with color red
    drawQuad(programRed, "a_position", 0, 0.5);
    EXPECT_GL_NO_ERROR();

    std::vector<GLColor> actualColors(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actualColors.data());
    for (int x = 0; x < getWindowWidth(); x++)
    {
        for (int y = 0; y < getWindowHeight(); y++)
        {
            const int currentPosition = y * getWindowHeight() + x;

            if (x >= getWindowWidth() / 4 && x < getWindowWidth() * 3 / 4 &&
                y >= getWindowHeight() / 4 && y < getWindowHeight() * 3 / 4)
            {
                EXPECT_COLOR_NEAR(GLColor(127, 127, 127, 127), actualColors[currentPosition], 1);
            }
            else
            {
                EXPECT_EQ(GLColor::blue, actualColors[currentPosition]);
            }
        }
    }
}

// Read both clip and cull distance varyings in fragment shaders
TEST_P(ClipCullDistanceTest, ClipCullInterpolation)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance"));

    constexpr char kVS[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require
in vec2 a_position;
void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    gl_ClipDistance[0] = dot(gl_Position, vec4( 1,  0, 0, 0.5));
    gl_ClipDistance[1] = dot(gl_Position, vec4(-1,  0, 0, 0.5));
    gl_ClipDistance[2] = dot(gl_Position, vec4( 0,  1, 0, 0.5));
    gl_ClipDistance[3] = dot(gl_Position, vec4( 0, -1, 0, 0.5));
    gl_CullDistance[0] = dot(gl_Position, vec4( 1,  0, 0, 1));
    gl_CullDistance[1] = dot(gl_Position, vec4(-1,  0, 0, 1));
    gl_CullDistance[2] = dot(gl_Position, vec4( 0,  1, 0, 1));
    gl_CullDistance[3] = dot(gl_Position, vec4( 0, -1, 0, 1));
})";

    constexpr char kFS[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require
precision highp float;
out vec4 my_FragColor;
void main()
{
    my_FragColor =
        vec4(gl_ClipDistance[0] + gl_ClipDistance[1],
             gl_ClipDistance[2] + gl_ClipDistance[3],
             gl_CullDistance[0] + gl_CullDistance[1],
             gl_CullDistance[2] + gl_CullDistance[3]) *
        vec4(0.5, 0.5, 0.25, 0.25);
})";

    ANGLE_GL_PROGRAM(programRed, kVS, kFS);
    glLinkProgram(programRed);
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    glEnable(GL_CLIP_DISTANCE0_EXT);
    glEnable(GL_CLIP_DISTANCE1_EXT);
    glEnable(GL_CLIP_DISTANCE2_EXT);
    glEnable(GL_CLIP_DISTANCE3_EXT);

    // Clear to blue
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw full screen quad with color red
    drawQuad(programRed, "a_position", 0);
    EXPECT_GL_NO_ERROR();

    std::vector<GLColor> actualColors(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actualColors.data());
    for (int x = 0; x < getWindowWidth(); x++)
    {
        for (int y = 0; y < getWindowHeight(); y++)
        {
            const int currentPosition = y * getWindowHeight() + x;

            if (x >= getWindowWidth() / 4 && x < getWindowWidth() * 3 / 4 &&
                y >= getWindowHeight() / 4 && y < getWindowHeight() * 3 / 4)
            {
                EXPECT_COLOR_NEAR(GLColor(127, 127, 127, 127), actualColors[currentPosition], 1);
            }
            else
            {
                EXPECT_EQ(GLColor::blue, actualColors[currentPosition]);
            }
        }
    }
}

// Write to 4 clip distances
TEST_P(ClipCullDistanceTest, FourClipDistances)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance"));

    constexpr char kVS[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require

in vec2 a_position;
uniform vec4 u_plane[4];

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    gl_ClipDistance[0] = dot(gl_Position, u_plane[0]);
    gl_ClipDistance[1] = dot(gl_Position, u_plane[1]);
    gl_ClipDistance[2] = dot(gl_Position, u_plane[2]);
    gl_ClipDistance[3] = dot(gl_Position, u_plane[3]);
})";

    ANGLE_GL_PROGRAM(programRed, kVS, essl3_shaders::fs::Red());
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    // Enable 3 clip distances
    glEnable(GL_CLIP_DISTANCE0_EXT);
    glEnable(GL_CLIP_DISTANCE2_EXT);
    glEnable(GL_CLIP_DISTANCE3_EXT);
    ASSERT_GL_NO_ERROR();

    // Disable 1 clip distances
    glDisable(GL_CLIP_DISTANCE1_EXT);
    ASSERT_GL_NO_ERROR();

    // Clear to blue
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    constexpr unsigned int kNumVertices                  = 12;
    const std::array<Vector3, kNumVertices> quadVertices = {
        {Vector3(-1.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 1.0f, 0.0f),
         Vector3(1.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, -1.0f, 0.0f),
         Vector3(1.0f, -1.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(-1.0f, -1.0f, 0.0f),
         Vector3(-1.0f, -1.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(-1.0f, 1.0f, 0.0f)}};

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * kNumVertices, quadVertices.data(),
                 GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    GLint positionLocation = glGetAttribLocation(programRed, "a_position");
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    ASSERT_GL_NO_ERROR();

    glEnableVertexAttribArray(positionLocation);
    ASSERT_GL_NO_ERROR();

    // Draw full screen quad and small size triangle with color red
    // y <= 1.0f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[0]"), 0, -1, 0, 1);
    // y >= 0.5f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[1]"), 0, 1, 0, -0.5);
    // y >= 3x-0.5f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[2]"), -3, 1, 0, 0.5);
    // y >= -3x-0.5f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[3]"), 3, 1, 0, 0.5);
    EXPECT_GL_NO_ERROR();

    glDrawArrays(GL_TRIANGLES, 0, kNumVertices);
    EXPECT_GL_NO_ERROR();

    const int windowWidth   = getWindowWidth();
    const int windowHeight  = getWindowHeight();
    auto checkLeftPlaneFunc = [windowWidth, windowHeight](int x, int y) -> float {
        return (3 * (x - (windowWidth / 2 - 1)) - (windowHeight / 4 + 1) + y);
    };
    auto checkRightPlaneFunc = [windowWidth, windowHeight](int x, int y) -> float {
        return (-3 * (x - (windowWidth / 2)) - (windowHeight / 4 + 1) + y);
    };

    // Only pixels in the triangle must be red
    std::vector<GLColor> actualColors(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actualColors.data());
    for (int x = 0; x < getWindowWidth(); ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            // The drawing method of Swiftshader and Native graphic card is different. So the
            // compare function doesn't check the value on the line.
            const int currentPosition = y * getWindowHeight() + x;

            if (checkLeftPlaneFunc(x, y) > 0 && checkRightPlaneFunc(x, y) > 0)
            {
                EXPECT_EQ(GLColor::red, actualColors[currentPosition]);
            }
            else if (checkLeftPlaneFunc(x, y) < 0 || checkRightPlaneFunc(x, y) < 0)
            {
                EXPECT_EQ(GLColor::blue, actualColors[currentPosition]);
            }
        }
    }
}

// Write to 4 cull distances
TEST_P(ClipCullDistanceTest, FourCullDistances)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance"));

    // SwiftShader bug: http://anglebug.com/5451
    ANGLE_SKIP_TEST_IF(isSwiftshader());

    constexpr char kVS[] = R"(#version 300 es
#extension GL_EXT_clip_cull_distance : require

uniform vec4 u_plane[4];

in vec2 a_position;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    gl_CullDistance[0] = dot(gl_Position, u_plane[0]);
    gl_CullDistance[1] = dot(gl_Position, u_plane[1]);
    gl_CullDistance[2] = dot(gl_Position, u_plane[2]);
    gl_CullDistance[3] = dot(gl_Position, u_plane[3]);
})";

    ANGLE_GL_PROGRAM(programRed, kVS, essl3_shaders::fs::Red());
    glLinkProgram(programRed);
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    // Clear to blue
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    constexpr unsigned int kNumVertices                  = 12;
    const std::array<Vector2, kNumVertices> quadVertices = {
        {Vector2(-1.0f, 1.0f), Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f), Vector2(1.0f, 1.0f),
         Vector2(0.0f, 0.0f), Vector2(1.0f, -1.0f), Vector2(1.0f, -1.0f), Vector2(0.0f, 0.0f),
         Vector2(-1.0f, -1.0f), Vector2(-1.0f, -1.0f), Vector2(0.0f, 0.0f), Vector2(-1.0f, 1.0f)}};

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * kNumVertices, quadVertices.data(),
                 GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    GLint positionLocation = glGetAttribLocation(programRed, "a_position");
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);
    ASSERT_GL_NO_ERROR();

    glEnableVertexAttribArray(positionLocation);
    ASSERT_GL_NO_ERROR();

    // Draw full screen quad and small size triangle with color red
    // y <= 1.0f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[0]"), 0, -1, 0, 1);
    // y >= 0.5f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[1]"), 0, 1, 0, -0.5);
    // y >= 3x-0.5f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[2]"), -3, 1, 0, 0.5);
    // y >= -3x-0.5f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[3]"), 3, 1, 0, 0.5);
    EXPECT_GL_NO_ERROR();

    glDrawArrays(GL_TRIANGLES, 0, kNumVertices);
    EXPECT_GL_NO_ERROR();

    // Only the bottom triangle must be culled
    std::vector<GLColor> actualColors(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actualColors.data());
    for (int x = 0; x < getWindowWidth(); ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            const int currentPosition = y * getWindowHeight() + x;

            if (y > x || y >= -x + getWindowHeight() - 1)
            {
                EXPECT_EQ(GLColor::red, actualColors[currentPosition]);
            }
            else
            {
                EXPECT_EQ(GLColor::blue, actualColors[currentPosition]);
            }
        }
    }
}

// Verify that EXT_clip_cull_distance works with EXT_geometry_shader
TEST_P(ClipCullDistanceTest, ClipDistanceInteractWithGeometryShader)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance") ||
                       !IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    constexpr char kVS[] = R"(#version 310 es
#extension GL_EXT_clip_cull_distance : require
#extension GL_EXT_geometry_shader : require

in vec2 a_position;
uniform vec4 u_plane[4];

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    gl_ClipDistance[0] = dot(gl_Position, u_plane[0]);
    gl_ClipDistance[1] = dot(gl_Position, u_plane[1]);
    gl_ClipDistance[2] = dot(gl_Position, u_plane[2]);
    gl_ClipDistance[3] = dot(gl_Position, u_plane[3]);
})";

    constexpr char kGS[] = R"(#version 310 es
#extension GL_EXT_clip_cull_distance : require
#extension GL_EXT_geometry_shader : require

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in gl_PerVertex {
    highp vec4 gl_Position;
    highp float gl_ClipDistance[];
} gl_in[];

out gl_PerVertex {
    highp vec4 gl_Position;
    highp float gl_ClipDistance[];
};

uniform vec4 u_plane[4];

void GetNewPosition(int i)
{
    gl_Position = 2.0f * gl_in[i].gl_Position;

    for (int index = 0 ; index < 4 ; index++)
    {
        if (gl_in[i].gl_ClipDistance[index] < 0.0f)
        {
            gl_ClipDistance[index] = dot(gl_Position, u_plane[index]);
        }
    }
    EmitVertex();
}

void main()
{
    for (int i = 0 ; i < 3 ; i++)
    {
        GetNewPosition(i);
    }
    EndPrimitive();
})";

    ANGLE_GL_PROGRAM_WITH_GS(programRed, kVS, kGS, essl31_shaders::fs::Red());
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    // Enable 3 clip distances
    glEnable(GL_CLIP_DISTANCE0_EXT);
    glEnable(GL_CLIP_DISTANCE2_EXT);
    glEnable(GL_CLIP_DISTANCE3_EXT);
    ASSERT_GL_NO_ERROR();

    // Disable 1 clip distances
    glDisable(GL_CLIP_DISTANCE1_EXT);
    ASSERT_GL_NO_ERROR();

    // Clear to blue
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    constexpr unsigned int kNumVertices                  = 12;
    const std::array<Vector3, kNumVertices> quadVertices = {
        {Vector3(-0.5f, 0.5f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.5f, 0.5f, 0.0f),
         Vector3(0.5f, 0.5f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.5f, -0.5f, 0.0f),
         Vector3(0.5f, -0.5f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(-0.5f, -0.5f, 0.0f),
         Vector3(-0.5f, -0.5f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(-0.5f, 0.5f, 0.0f)}};

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * kNumVertices, quadVertices.data(),
                 GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    GLint positionLocation = glGetAttribLocation(programRed, "a_position");
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    ASSERT_GL_NO_ERROR();

    glEnableVertexAttribArray(positionLocation);
    ASSERT_GL_NO_ERROR();

    // Draw full screen quad and small size triangle with color red
    // y <= 1.0f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[0]"), 0, -1, 0, 1);
    // y >= 0.5f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[1]"), 0, 1, 0, -0.5);
    // y >= 3x-0.5f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[2]"), -3, 1, 0, 0.5);
    // y >= -3x-0.5f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[3]"), 3, 1, 0, 0.5);
    EXPECT_GL_NO_ERROR();

    glDrawArrays(GL_TRIANGLES, 0, kNumVertices);
    EXPECT_GL_NO_ERROR();

    const int windowWidth   = getWindowWidth();
    const int windowHeight  = getWindowHeight();
    auto checkLeftPlaneFunc = [windowWidth, windowHeight](int x, int y) -> float {
        return (3 * (x - (windowWidth / 2 - 1)) - (windowHeight / 4 + 1) + y);
    };
    auto checkRightPlaneFunc = [windowWidth, windowHeight](int x, int y) -> float {
        return (-3 * (x - (windowWidth / 2)) - (windowHeight / 4 + 1) + y);
    };

    // Only pixels in the triangle must be red
    std::vector<GLColor> actualColors(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actualColors.data());
    for (int x = 0; x < getWindowWidth(); ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            // The drawing method of Swiftshader and Native graphic card is different. So the
            // compare function doesn't check the value on the line.
            const int currentPosition = y * getWindowHeight() + x;

            if (checkLeftPlaneFunc(x, y) > 0 && checkRightPlaneFunc(x, y) > 0)
            {
                EXPECT_EQ(GLColor::red, actualColors[currentPosition]);
            }
            else if (checkLeftPlaneFunc(x, y) < 0 || checkRightPlaneFunc(x, y) < 0)
            {
                EXPECT_EQ(GLColor::blue, actualColors[currentPosition]);
            }
        }
    }
}

// Verify that EXT_clip_cull_distance works with EXT_geometry_shader
TEST_P(ClipCullDistanceTest, CullDistanceInteractWithGeometryShader)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_clip_cull_distance") ||
                       !IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    constexpr char kVS[] = R"(#version 310 es
#extension GL_EXT_clip_cull_distance : require
#extension GL_EXT_geometry_shader : require

in vec2 a_position;
uniform vec4 u_plane[4];

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    gl_CullDistance[0] = dot(gl_Position, u_plane[0]);
    gl_CullDistance[1] = dot(gl_Position, u_plane[1]);
    gl_CullDistance[2] = dot(gl_Position, u_plane[2]);
    gl_CullDistance[3] = dot(gl_Position, u_plane[3]);
})";

    constexpr char kGS[] = R"(#version 310 es
#extension GL_EXT_clip_cull_distance : require
#extension GL_EXT_geometry_shader : require

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in gl_PerVertex {
    highp vec4 gl_Position;
    highp float gl_CullDistance[];
} gl_in[];

out gl_PerVertex {
    highp vec4 gl_Position;
    highp float gl_CullDistance[];
};

uniform vec4 u_plane[4];

void GetNewPosition(int i)
{
    gl_Position = 2.0f * gl_in[i].gl_Position;

    for (int index = 0 ; index < 4 ; index++)
    {
        if (gl_in[i].gl_CullDistance[index] < 0.0f)
        {
            gl_CullDistance[index] = dot(gl_Position, u_plane[index]);
        }
    }
    EmitVertex();
}

void main()
{
    for (int i = 0 ; i < 3 ; i++)
    {
        GetNewPosition(i);
    }
    EndPrimitive();
})";

    ANGLE_GL_PROGRAM_WITH_GS(programRed, kVS, kGS, essl31_shaders::fs::Red());
    glUseProgram(programRed);
    ASSERT_GL_NO_ERROR();

    // Clear to blue
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    constexpr unsigned int kNumVertices                  = 12;
    const std::array<Vector3, kNumVertices> quadVertices = {
        {Vector3(-0.5f, 0.5f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.5f, 0.5f, 0.0f),
         Vector3(0.5f, 0.5f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.5f, -0.5f, 0.0f),
         Vector3(0.5f, -0.5f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(-0.5f, -0.5f, 0.0f),
         Vector3(-0.5f, -0.5f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(-0.5f, 0.5f, 0.0f)}};

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * kNumVertices, quadVertices.data(),
                 GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    GLint positionLocation = glGetAttribLocation(programRed, "a_position");
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    ASSERT_GL_NO_ERROR();

    glEnableVertexAttribArray(positionLocation);
    ASSERT_GL_NO_ERROR();

    // Draw full screen quad and small size triangle with color red
    // y <= 1.0f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[0]"), 0, -1, 0, 1);
    // y >= 0.5f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[1]"), 0, 1, 0, -0.5);
    // y >= 3x-0.5f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[2]"), -3, 1, 0, 0.5);
    // y >= -3x-0.5f
    glUniform4f(glGetUniformLocation(programRed, "u_plane[3]"), 3, 1, 0, 0.5);
    EXPECT_GL_NO_ERROR();

    glDrawArrays(GL_TRIANGLES, 0, kNumVertices);
    EXPECT_GL_NO_ERROR();

    // Only pixels in the triangle must be red
    std::vector<GLColor> actualColors(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actualColors.data());
    for (int x = 0; x < getWindowWidth(); ++x)
    {
        for (int y = 0; y < getWindowHeight(); ++y)
        {
            const int currentPosition = y * getWindowHeight() + x;

            if (y > x || y >= -x + getWindowHeight() - 1)
            {
                EXPECT_EQ(GLColor::red, actualColors[currentPosition]);
            }
            else
            {
                EXPECT_EQ(GLColor::blue, actualColors[currentPosition]);
            }
        }
    }
}

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(ClipDistanceTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ClipCullDistanceTest);
ANGLE_INSTANTIATE_TEST_ES3_AND_ES31(ClipCullDistanceTest);
