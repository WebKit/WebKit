//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RobustBufferAccessBehaviorTest:
//   Various tests related for GL_KHR_robust_buffer_access_behavior.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class RobustBufferAccessBehaviorTest : public ANGLETest
{
  protected:
    RobustBufferAccessBehaviorTest() : mProgram(0), mTestAttrib(-1)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void TearDown() override
    {
        glDeleteProgram(mProgram);
        ANGLETest::TearDown();
    }

    bool initExtension()
    {
        EGLWindow *window  = getEGLWindow();
        EGLDisplay display = window->getDisplay();
        if (!eglDisplayExtensionEnabled(display, "EGL_EXT_create_context_robustness"))
        {
            return false;
        }

        ANGLETest::TearDown();
        setRobustAccess(true);
        ANGLETest::SetUp();
        if (!extensionEnabled("GL_KHR_robust_buffer_access_behavior"))
        {
            return false;
        }
        return true;
    }

    void initBasicProgram()
    {
        const std::string &vsCheckOutOfBounds =
            "precision mediump float;\n"
            "attribute vec4 position;\n"
            "attribute vec4 vecRandom;\n"
            "varying vec4 v_color;\n"
            "bool testFloatComponent(float component) {\n"
            "    return (component == 0.2 || component == 0.0);\n"
            "}\n"
            "bool testLastFloatComponent(float component) {\n"
            "    return testFloatComponent(component) || component == 1.0;\n"
            "}\n"
            "void main() {\n"
            "    if (testFloatComponent(vecRandom.x) &&\n"
            "        testFloatComponent(vecRandom.y) &&\n"
            "        testFloatComponent(vecRandom.z) &&\n"
            "        testLastFloatComponent(vecRandom.w)) {\n"
            "        v_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
            "    } else {\n"
            "        v_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
            "    }\n"
            "    gl_Position = position;\n"
            "}\n";

        const std::string &fragmentShaderSource =
            "precision mediump float;\n"
            "varying vec4 v_color;\n"
            "void main() {\n"
            "    gl_FragColor = v_color;\n"
            "}\n";

        mProgram = CompileProgram(vsCheckOutOfBounds, fragmentShaderSource);
        ASSERT_NE(0u, mProgram);

        mTestAttrib = glGetAttribLocation(mProgram, "vecRandom");
        ASSERT_NE(-1, mTestAttrib);

        glUseProgram(mProgram);
    }

    void runIndexOutOfRangeTests(GLenum drawType)
    {
        if (mProgram == 0)
        {
            initBasicProgram();
        }

        GLBuffer bufferIncomplete;
        glBindBuffer(GL_ARRAY_BUFFER, bufferIncomplete);
        std::array<GLfloat, 12> randomData = {
            {0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f}};
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * randomData.size(), randomData.data(),
                     drawType);

        glEnableVertexAttribArray(mTestAttrib);
        glVertexAttribPointer(mTestAttrib, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

        glClearColor(0.0, 0.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        drawIndexedQuad(mProgram, "position", 0.5f);

        int width     = getWindowWidth();
        int height    = getWindowHeight();
        GLenum result = glGetError();
        // For D3D dynamic draw, we still return invalid operation. Once we force the index buffer
        // to clamp any out of range indices instead of invalid operation, this part can be removed.
        // We can always get GL_NO_ERROR.
        if (result == GL_INVALID_OPERATION)
        {
            EXPECT_PIXEL_COLOR_EQ(width * 1 / 4, height * 1 / 4, GLColor::blue);
            EXPECT_PIXEL_COLOR_EQ(width * 1 / 4, height * 3 / 4, GLColor::blue);
            EXPECT_PIXEL_COLOR_EQ(width * 3 / 4, height * 1 / 4, GLColor::blue);
            EXPECT_PIXEL_COLOR_EQ(width * 3 / 4, height * 3 / 4, GLColor::blue);
        }
        else
        {
            EXPECT_GLENUM_EQ(GL_NO_ERROR, result);
            EXPECT_PIXEL_COLOR_EQ(width * 1 / 4, height * 1 / 4, GLColor::green);
            EXPECT_PIXEL_COLOR_EQ(width * 1 / 4, height * 3 / 4, GLColor::green);
            EXPECT_PIXEL_COLOR_EQ(width * 3 / 4, height * 1 / 4, GLColor::green);
            EXPECT_PIXEL_COLOR_EQ(width * 3 / 4, height * 3 / 4, GLColor::green);
        }
    }

    GLuint mProgram;
    GLint mTestAttrib;
};

// Test that static draw with out-of-bounds reads will not read outside of the data store of the
// buffer object and will not result in GL interruption or termination when
// GL_KHR_robust_buffer_access_behavior is supported.
TEST_P(RobustBufferAccessBehaviorTest, DrawElementsIndexOutOfRangeWithStaticDraw)
{
    ANGLE_SKIP_TEST_IF(IsNVIDIA() && IsWindows() && IsOpenGL());
    ANGLE_SKIP_TEST_IF(!initExtension());

    runIndexOutOfRangeTests(GL_STATIC_DRAW);
}

// Test that dynamic draw with out-of-bounds reads will not read outside of the data store of the
// buffer object and will not result in GL interruption or termination when
// GL_KHR_robust_buffer_access_behavior is supported.
TEST_P(RobustBufferAccessBehaviorTest, DrawElementsIndexOutOfRangeWithDynamicDraw)
{
    ANGLE_SKIP_TEST_IF(IsNVIDIA() && IsWindows() && IsOpenGL());
    ANGLE_SKIP_TEST_IF(!initExtension());

    runIndexOutOfRangeTests(GL_DYNAMIC_DRAW);
}

ANGLE_INSTANTIATE_TEST(RobustBufferAccessBehaviorTest,
                       ES2_D3D9(),
                       ES2_D3D11_FL9_3(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES31_D3D11(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES31_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES(),
                       ES31_OPENGLES());

}  // namespace
