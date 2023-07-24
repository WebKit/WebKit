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

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        // Not reached during capture as we hit the End frame earlier.

        if (!mFBOs.empty())
        {
            glDeleteFramebuffers(static_cast<GLsizei>(mFBOs.size()), mFBOs.data());
        }
    }

    void frame1();
    void frame2();

    std::vector<GLuint> mFBOs;
};

void CapturedTest::frame1()
{
    glClearColor(0.25f, 0.5f, 0.5f, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_NEAR(0, 0, 64, 128, 128, 128, 1.0);
}

void CapturedTest::frame2()
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

    // Empty frames to reach capture end.
    for (int i = 0; i < 10; i++)
    {
        swapBuffers();
    }
    // Note: test teardown adds an additonal swap in
    // ANGLETestBase::ANGLETestPreTearDown() when --angle-per-test-capture-label
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CapturedTest);
ANGLE_INSTANTIATE_TEST_ES3(CapturedTest);

}  // anonymous namespace
