//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

using namespace angle;

class BlendMinMaxTest : public ANGLETest
{
  protected:
    BlendMinMaxTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);

        mProgram = 0;
        mColorLocation = -1;
        mFramebuffer = 0;
        mColorRenderbuffer = 0;
    }

    struct Color
    {
        float values[4];
    };

    static float getExpected(bool blendMin, float curColor, float prevColor)
    {
        return blendMin ? std::min(curColor, prevColor) : std::max(curColor, prevColor);
    }

    void runTest(GLenum colorFormat, GLenum type)
    {
        if (getClientMajorVersion() < 3 && !extensionEnabled("GL_EXT_blend_minmax"))
        {
            std::cout << "Test skipped because ES3 or GL_EXT_blend_minmax is not available." << std::endl;
            return;
        }

        // TODO(geofflang): figure out why this fails
        if (IsIntel() && GetParam() == ES2_OPENGL())
        {
            std::cout << "Test skipped on OpenGL Intel due to flakyness." << std::endl;
            return;
        }

        SetUpFramebuffer(colorFormat);

        int minValue = 0;
        int maxValue = 1;
        if (type == GL_FLOAT)
        {
            minValue = -1024;
            maxValue = 1024;
        }

        const size_t colorCount = 128;
        Color colors[colorCount];
        for (size_t i = 0; i < colorCount; i++)
        {
            for (size_t j = 0; j < 4; j++)
            {
                colors[i].values[j] =
                    static_cast<float>(minValue + (rand() % (maxValue - minValue)));
            }
        }

        float prevColor[4];
        for (size_t i = 0; i < colorCount; i++)
        {
            const Color &color = colors[i];
            glUseProgram(mProgram);
            glUniform4f(mColorLocation, color.values[0], color.values[1], color.values[2], color.values[3]);

            bool blendMin = (rand() % 2 == 0);
            glBlendEquation(blendMin ? GL_MIN : GL_MAX);

            drawQuad(mProgram, "aPosition", 0.5f);

            float pixel[4];
            if (type == GL_UNSIGNED_BYTE)
            {
                GLubyte ubytePixel[4];
                glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, ubytePixel);
                for (size_t componentIdx = 0; componentIdx < ArraySize(pixel); componentIdx++)
                {
                    pixel[componentIdx] = ubytePixel[componentIdx] / 255.0f;
                }
            }
            else if (type == GL_FLOAT)
            {
                glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixel);
            }
            else
            {
                FAIL() << "Unexpected pixel type";
            }

            if (i > 0)
            {
                const float errorRange = 1.0f / 255.0f;
                for (size_t componentIdx = 0; componentIdx < ArraySize(pixel); componentIdx++)
                {
                    EXPECT_NEAR(
                        getExpected(blendMin, color.values[componentIdx], prevColor[componentIdx]),
                        pixel[componentIdx], errorRange);
                }
            }

            memcpy(prevColor, pixel, sizeof(pixel));
        }
    }

    virtual void SetUp()
    {
        ANGLETest::SetUp();

        const std::string testVertexShaderSource = SHADER_SOURCE
        (
            attribute highp vec4 aPosition;

            void main(void)
            {
                gl_Position = aPosition;
            }
        );

        const std::string testFragmentShaderSource = SHADER_SOURCE
        (
            uniform highp vec4 color;
            void main(void)
            {
                gl_FragColor = color;
            }
        );

        mProgram = CompileProgram(testVertexShaderSource, testFragmentShaderSource);
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        mColorLocation = glGetUniformLocation(mProgram, "color");

        glUseProgram(mProgram);

        glClearColor(0, 0, 0, 0);
        glClearDepthf(0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
    }

    void SetUpFramebuffer(GLenum colorFormat)
    {
        glGenFramebuffers(1, &mFramebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFramebuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, mFramebuffer);

        glGenRenderbuffers(1, &mColorRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, mColorRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, colorFormat, getWindowWidth(), getWindowHeight());
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mColorRenderbuffer);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ASSERT_GL_NO_ERROR();
    }

    virtual void TearDown()
    {
        glDeleteProgram(mProgram);
        glDeleteFramebuffers(1, &mFramebuffer);
        glDeleteRenderbuffers(1, &mColorRenderbuffer);

        ANGLETest::TearDown();
    }

    GLuint mProgram;
    GLint mColorLocation;

    GLuint mFramebuffer;
    GLuint mColorRenderbuffer;
};

TEST_P(BlendMinMaxTest, RGBA8)
{
    runTest(GL_RGBA8, GL_UNSIGNED_BYTE);
}

TEST_P(BlendMinMaxTest, RGBA32F)
{
    if (getClientMajorVersion() < 3 || !extensionEnabled("GL_EXT_color_buffer_float"))
    {
        std::cout << "Test skipped because ES3 and GL_EXT_color_buffer_float are not available."
                  << std::endl;
        return;
    }

    // TODO(jmadill): Figure out why this is broken on Intel
    if (IsIntel() && (GetParam() == ES2_D3D11() || GetParam() == ES2_D3D9()))
    {
        std::cout << "Test skipped on Intel OpenGL." << std::endl;
        return;
    }

    // TODO (bug 1284): Investigate RGBA32f D3D SDK Layers messages on D3D11_FL9_3
    if (IsD3D11_FL93())
    {
        std::cout << "Test skipped on Feature Level 9_3." << std::endl;
        return;
    }

    runTest(GL_RGBA32F, GL_FLOAT);
}

TEST_P(BlendMinMaxTest, RGBA16F)
{
    if (getClientMajorVersion() < 3 && !extensionEnabled("GL_EXT_color_buffer_half_float"))
    {
        std::cout << "Test skipped because ES3 or GL_EXT_color_buffer_half_float is not available."
                  << std::endl;
        return;
    }

    // TODO(jmadill): figure out why this fails
    if (IsIntel() && (GetParam() == ES2_D3D11() || GetParam() == ES2_D3D9()))
    {
        std::cout << "Test skipped on Intel due to failures." << std::endl;
        return;
    }

    runTest(GL_RGBA16F, GL_FLOAT);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
ANGLE_INSTANTIATE_TEST(BlendMinMaxTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_D3D11_FL9_3(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES());
