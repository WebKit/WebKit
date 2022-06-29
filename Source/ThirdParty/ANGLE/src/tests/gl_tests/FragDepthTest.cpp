//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FragDepthTest:
//   Tests the correctness of gl_FragDepth usage.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

class FragDepthTest : public ANGLETest<>
{
  protected:
    struct FragDepthTestResources
    {
        GLuint program;
        GLint depthLocation;
        GLTexture colorTexture;
        GLTexture depthTexture;
        GLFramebuffer framebuffer;
    };

    void setupResources(FragDepthTestResources &resources)
    {

        // Writes a fixed depth value and green.
        constexpr char kFS[] =
            R"(#version 300 es
            precision highp float;
            layout(location = 0) out vec4 fragColor;
            uniform float u_depth;
            void main(){
                gl_FragDepth = u_depth;
                fragColor = vec4(0.0, 1.0, 0.0, 1.0);
            })";

        resources.program       = CompileProgram(essl3_shaders::vs::Simple(), kFS);
        resources.depthLocation = glGetUniformLocation(resources.program, "u_depth");

        glBindTexture(GL_TEXTURE_2D, resources.colorTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D, resources.depthTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT32F, 1, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, resources.framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               resources.colorTexture, 0);
    }

    void cleanupResources(FragDepthTestResources &resources) { glDeleteProgram(resources.program); }

    void checkDepthWritten(float expectedDepth, float fsDepth, bool bindDepthBuffer)
    {
        FragDepthTestResources resources;
        setupResources(resources);
        ASSERT_NE(0u, resources.program);
        ASSERT_NE(-1, resources.depthLocation);
        ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
        ASSERT_GL_NO_ERROR();

        glBindFramebuffer(GL_FRAMEBUFFER, resources.framebuffer);
        if (bindDepthBuffer)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                   resources.depthTexture, 0);
            EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

            // Clear to the expected depth so it will be compared to the FS depth with
            // DepthFunc(GL_EQUAL)
            glClearDepthf(expectedDepth);
            glDepthFunc(GL_EQUAL);
            glEnable(GL_DEPTH_TEST);
        }
        else
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
            EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
            glDisable(GL_DEPTH_TEST);
        }
        glUseProgram(resources.program);

        // Clear to red, the FS will write green on success
        glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
        // Clear to the expected depth so it will be compared to the FS depth with
        // DepthFunc(GL_EQUAL)

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUniform1f(resources.depthLocation, fsDepth);

        drawQuad(resources.program, "a_position", 0.0f);
        EXPECT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
        cleanupResources(resources);
    }
};

// Test that writing to gl_FragDepth works
TEST_P(FragDepthTest, DepthBufferBound)
{
    checkDepthWritten(0.5f, 0.5f, true);
}

// Test that writing to gl_FragDepth with no depth buffer works.
TEST_P(FragDepthTest, DepthBufferUnbound)
{
    // Depth test is disabled, so the expected depth should not matter.
    checkDepthWritten(0.f, 0.5f, false);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(FragDepthTest);
ANGLE_INSTANTIATE_TEST_ES3(FragDepthTest);
