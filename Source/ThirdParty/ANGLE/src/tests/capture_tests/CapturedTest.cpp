//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"
#include "util/random_utils.h"
#include "util/shader_utils.h"
#include "util/test_utils.h"

using namespace angle;

namespace
{
class CapturedTest : public ANGLETest<>
{
  protected:
    CapturedTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
        setConfigStencilBits(8);
    }

    void testSetUp() override
    {
        // Calls not captured because we setup Start frame to MEC.

        // TODO: why are these framebuffers not showing up in the capture?

        mFBOs.resize(2, 0);
        glGenFramebuffers(2, mFBOs.data());

        constexpr char kInactiveVS[] = R"(precision highp float;
void main(void) {
   gl_Position = vec4(0.5, 0.5, 0.5, 1.0);
})";

        // Create shader that is unused during capture
        inactiveProgram                    = glCreateProgram();
        inactiveShader                     = glCreateShader(GL_VERTEX_SHADER);
        const char *inactiveSourceArray[1] = {kInactiveVS};
        glShaderSource(inactiveShader, 1, inactiveSourceArray, 0);
        glCompileShader(inactiveShader);
        glAttachShader(inactiveProgram, inactiveShader);

        // Create Shader Program before capture begins to use during capture
        activeBeforeProgram                = glCreateProgram();
        activeBeforeVertShader             = glCreateShader(GL_VERTEX_SHADER);
        activeBeforeFragShader             = glCreateShader(GL_FRAGMENT_SHADER);
        const char *activeVsSourceArray[1] = {kActiveVS};
        glShaderSource(activeBeforeVertShader, 1, activeVsSourceArray, 0);
        glCompileShader(activeBeforeVertShader);
        glAttachShader(activeBeforeProgram, activeBeforeVertShader);
        const char *activeFsSourceArray[1] = {kActiveFS};
        glShaderSource(activeBeforeFragShader, 1, activeFsSourceArray, 0);
        glCompileShader(activeBeforeFragShader);
        glAttachShader(activeBeforeProgram, activeBeforeFragShader);
        glLinkProgram(activeBeforeProgram);

        // Get the attr/sampler locations
        mPositionLoc = glGetAttribLocation(activeBeforeProgram, "a_position");
        mTexCoordLoc = glGetAttribLocation(activeBeforeProgram, "a_texCoord");
        mSamplerLoc  = glGetUniformLocation(activeBeforeProgram, "s_texture");
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // Bind the texture object before capture begins
        glGenTextures(1, &textureNeverBound);
        glGenTextures(1, &textureBoundBeforeCapture);
        glBindTexture(GL_TEXTURE_2D, textureBoundBeforeCapture);
        ASSERT_GL_NO_ERROR();

        const size_t width                 = 2;
        const size_t height                = 2;
        GLubyte pixels[width * height * 3] = {
            255, 0,   0,    // Red
            0,   255, 0,    // Green
            0,   0,   255,  // Blue
            255, 255, 0,    // Yellow
        };
        // Populate the pre-capture bound texture
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    void testTearDown() override
    {
        // Not reached during capture as we hit the End frame earlier.

        if (!mFBOs.empty())
        {
            glDeleteFramebuffers(static_cast<GLsizei>(mFBOs.size()), mFBOs.data());
        }

        glDeleteTextures(1, &textureNeverBound);
        glDeleteTextures(1, &textureBoundBeforeCapture);
        glDeleteProgram(inactiveProgram);
        glDeleteProgram(activeBeforeProgram);
        glDeleteShader(inactiveShader);
        glDeleteShader(activeBeforeVertShader);
        glDeleteShader(activeBeforeFragShader);
    }

    static constexpr char kActiveVS[] = R"(attribute vec4 a_position;
attribute vec2 a_texCoord;
varying vec2 v_texCoord;
void main()
{
    gl_Position = a_position;
    v_texCoord = a_texCoord;
})";

    static constexpr char kActiveFS[] = R"(precision mediump float;
varying vec2 v_texCoord;
uniform sampler2D s_texture;
void main()
{
    gl_FragColor = texture2D(s_texture, v_texCoord);
})";

    void frame1();
    void frame2();
    void frame3();

    std::vector<GLuint> mFBOs;

    GLuint inactiveProgram;
    GLuint inactiveShader;

    GLuint activeBeforeProgram;
    GLuint activeBeforeVertShader;
    GLuint activeBeforeFragShader;

    GLuint textureNeverBound;
    GLuint textureBoundBeforeCapture;
    GLuint mPositionLoc;
    GLuint mTexCoordLoc;
    GLint mSamplerLoc;
};

void CapturedTest::frame1()
{
    glClearColor(0.25f, 0.5f, 0.5f, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_NEAR(0, 0, 64, 128, 128, 128, 1.0);

    GLfloat vertices[] = {
        -0.75f, 0.25f,  0.0f,  // Position 0
        0.0f,   0.0f,          // TexCoord 0
        -0.75f, -0.75f, 0.0f,  // Position 1
        0.0f,   1.0f,          // TexCoord 1
        0.25f,  -0.75f, 0.0f,  // Position 2
        1.0f,   1.0f,          // TexCoord 2
        0.25f,  0.25f,  0.0f,  // Position 3
        1.0f,   0.0f           // TexCoord 3
    };

    GLushort indices[] = {0, 1, 2, 0, 2, 3};

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(activeBeforeProgram);
    glVertexAttribPointer(mPositionLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vertices);
    glVertexAttribPointer(mTexCoordLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vertices + 3);
    glEnableVertexAttribArray(mPositionLoc);
    glEnableVertexAttribArray(mTexCoordLoc);
    glUniform1i(mSamplerLoc, 0);

    // Draw without binding texture during capture
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    EXPECT_PIXEL_EQ(20, 20, 0, 0, 255, 255);

    glDeleteVertexArrays(1, &mPositionLoc);
    glDeleteVertexArrays(1, &mTexCoordLoc);
}

void CapturedTest::frame2()
{
    // Draw using texture created and bound during capture

    GLuint activeDuringProgram;
    GLuint activeDuringVertShader;
    GLuint activeDuringFragShader;
    GLuint positionLoc;
    GLuint texCoordLoc;
    GLuint textureBoundDuringCapture;
    GLint samplerLoc;

    activeDuringProgram                = glCreateProgram();
    activeDuringVertShader             = glCreateShader(GL_VERTEX_SHADER);
    activeDuringFragShader             = glCreateShader(GL_FRAGMENT_SHADER);
    const char *activeVsSourceArray[1] = {kActiveVS};
    glShaderSource(activeDuringVertShader, 1, activeVsSourceArray, 0);
    glCompileShader(activeDuringVertShader);
    glAttachShader(activeDuringProgram, activeDuringVertShader);
    const char *activeFsSourceArray[1] = {kActiveFS};
    glShaderSource(activeDuringFragShader, 1, activeFsSourceArray, 0);
    glCompileShader(activeDuringFragShader);
    glAttachShader(activeDuringProgram, activeDuringFragShader);
    glLinkProgram(activeDuringProgram);

    // Get the attr/sampler locations
    positionLoc = glGetAttribLocation(activeDuringProgram, "a_position");
    texCoordLoc = glGetAttribLocation(activeDuringProgram, "a_texCoord");
    samplerLoc  = glGetUniformLocation(activeDuringProgram, "s_texture");
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Bind the texture object during capture
    glGenTextures(1, &textureBoundDuringCapture);
    glBindTexture(GL_TEXTURE_2D, textureBoundDuringCapture);
    ASSERT_GL_NO_ERROR();

    const size_t width                 = 2;
    const size_t height                = 2;
    GLubyte pixels[width * height * 3] = {
        255, 255, 0,    // Yellow
        0,   0,   255,  // Blue
        0,   255, 0,    // Green
        255, 0,   0,    // Red
    };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLfloat vertices[] = {
        -0.25f, 0.75f,  0.0f,  // Position 0
        0.0f,   0.0f,          // TexCoord 0
        -0.25f, -0.25f, 0.0f,  // Position 1
        0.0f,   1.0f,          // TexCoord 1
        0.75f,  -0.25f, 0.0f,  // Position 2
        1.0f,   1.0f,          // TexCoord 2
        0.75f,  0.75f,  0.0f,  // Position 3
        1.0f,   0.0f           // TexCoord 3
    };
    GLushort indices[] = {0, 1, 2, 0, 2, 3};

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(activeDuringProgram);
    glVertexAttribPointer(positionLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vertices);
    glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vertices + 3);
    glEnableVertexAttribArray(positionLoc);
    glEnableVertexAttribArray(texCoordLoc);
    glUniform1i(samplerLoc, 0);

    // Draw using texture bound during capture
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    EXPECT_PIXEL_EQ(108, 108, 0, 0, 255, 255);

    glDeleteTextures(1, &textureBoundDuringCapture);
    glDeleteVertexArrays(1, &positionLoc);
    glDeleteVertexArrays(1, &texCoordLoc);
    glDeleteProgram(activeDuringProgram);
    glDeleteShader(activeDuringVertShader);
    glDeleteShader(activeDuringFragShader);
}

void CapturedTest::frame3()
{
    // TODO: using local objects (with RAII helpers) here that create and destroy objects within the
    // frame. Maybe move some of this to test Setup.

    constexpr char kVS[] = R"(precision highp float;
attribute vec3 attr1;
void main(void) {
   gl_Position = vec4(attr1, 1.0);
})";

    constexpr char kFS[] = R"(precision highp float;
void main(void) {
   gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
})";

    GLBuffer emptyBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, emptyBuffer);

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glBindAttribLocation(program, 0, "attr1");
    glLinkProgram(program);
    ASSERT_TRUE(CheckLinkStatusAndReturnProgram(program, true));
    glUseProgram(program);

    // Use non-existing attribute 1.
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, false, 1, 0);

    glDrawArrays(GL_TRIANGLES, 0, 3);
    EXPECT_GL_NO_ERROR();

    // Note: RAII destructors called here causing additional GL calls.
}

// Test captured by capture_tests.py
TEST_P(CapturedTest, MultiFrame)
{
    // Swap before the first frame so that setup gets its own frame
    swapBuffers();
    frame1();

    swapBuffers();
    frame2();

    swapBuffers();
    frame3();

    // Empty frames to reach capture end.
    for (int i = 0; i < 10; i++)
    {
        swapBuffers();
    }
    // Note: test teardown adds an additonal swap in
    // ANGLETestBase::ANGLETestPreTearDown() when --angle-per-test-capture-label
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CapturedTest);
// Capture is only supported on the Vulkan backend
ANGLE_INSTANTIATE_TEST(CapturedTest, ES3_VULKAN());

}  // anonymous namespace
