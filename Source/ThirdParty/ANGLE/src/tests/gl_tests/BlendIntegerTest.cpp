//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

class BlendIntegerTest : public ANGLETest
{
  protected:
    BlendIntegerTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    template <typename T, GLuint components>
    void compareValue(const T *value, const char *name)
    {
        T pixel[4];
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER,
                     std::is_same<T, int32_t>::value ? GL_INT : GL_UNSIGNED_INT, pixel);
        for (size_t componentIdx = 0; componentIdx < components; componentIdx++)
        {
            EXPECT_EQ(value[componentIdx], pixel[componentIdx])
                << " componentIdx=" << componentIdx << std::endl
                << " " << name << "[0]=" << value[0] << " pixel[0]=" << pixel[0] << std::endl
                << " " << name << "[1]=" << value[1] << " pixel[1]=" << pixel[1] << std::endl
                << " " << name << "[2]=" << value[2] << " pixel[2]=" << pixel[2] << std::endl
                << " " << name << "[3]=" << value[3] << " pixel[3]=" << pixel[3];
        }
    }

    template <GLenum internalformat, GLuint components, bool isSigned>
    void runTest()
    {
        // https://crbug.com/angleproject/4548
        ANGLE_SKIP_TEST_IF(isVulkanRenderer() && IsIntel() && !isVulkanSwiftshaderRenderer());

        constexpr char kFsui[] =
            "#version 300 es\n"
            "out highp uvec4 o_drawBuffer0;\n"
            "void main(void)\n"
            "{\n"
            "    o_drawBuffer0 = uvec4(1, 1, 1, 1);\n"
            "}\n";

        constexpr char kFssi[] =
            "#version 300 es\n"
            "out highp ivec4 o_drawBuffer0;\n"
            "void main(void)\n"
            "{\n"
            "    o_drawBuffer0 = ivec4(-1, -1, -1, -1);\n"
            "}\n";

        ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), isSigned ? kFssi : kFsui);
        glUseProgram(program);

        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

        GLRenderbuffer colorRenderbuffer;
        glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, internalformat, getWindowWidth(), getWindowHeight());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  colorRenderbuffer);

        if (isSigned)
        {
            const int32_t clearValueSigned[4] = {-128, -128, -128, -128};
            glClearBufferiv(GL_COLOR, 0, clearValueSigned);
            ASSERT_GL_NO_ERROR();
            compareValue<int32_t, components>(clearValueSigned, "clearValueSigned");
        }
        else
        {
            const uint32_t clearValueUnsigned[4] = {127, 127, 127, 3};
            glClearBufferuiv(GL_COLOR, 0, clearValueUnsigned);
            ASSERT_GL_NO_ERROR();
            compareValue<uint32_t, components>(clearValueUnsigned, "clearValueUnsigned");
        }

        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ONE);

        drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);

        ASSERT_GL_NO_ERROR();

        // Enabled blending must be ignored for integer color attachment.
        if (isSigned)
        {
            const int32_t colorValueSigned[4] = {-1, -1, -1, -1};
            compareValue<int32_t, components>(colorValueSigned, "colorValueSigned");
        }
        else
        {
            const uint32_t colorValueUnsigned[4] = {1, 1, 1, 1};
            compareValue<uint32_t, components>(colorValueUnsigned, "colorValueUnsigned");
        }
    }
};

// Test that blending is not applied to signed integer attachments.
TEST_P(BlendIntegerTest, R8I)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_R8I, 1, true>();
}

TEST_P(BlendIntegerTest, R16I)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_R16I, 1, true>();
}

TEST_P(BlendIntegerTest, R32I)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_R32I, 1, true>();
}

TEST_P(BlendIntegerTest, RG8I)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_RG8I, 2, true>();
}

TEST_P(BlendIntegerTest, RG16I)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_RG16I, 2, true>();
}

TEST_P(BlendIntegerTest, RG32I)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_RG32I, 2, true>();
}

TEST_P(BlendIntegerTest, RGBA8I)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_RGBA8I, 4, true>();
}

TEST_P(BlendIntegerTest, RGBA16I)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_RGBA16I, 4, true>();
}

TEST_P(BlendIntegerTest, RGBA32I)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_RGBA32I, 4, true>();
}

// Test that blending is not applied to unsigned integer attachments.
TEST_P(BlendIntegerTest, R8UI)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_R8UI, 1, false>();
}

TEST_P(BlendIntegerTest, R16UI)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_R16UI, 1, false>();
}

TEST_P(BlendIntegerTest, R32UI)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_R32UI, 1, false>();
}

TEST_P(BlendIntegerTest, RG8UI)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_RG8UI, 2, false>();
}

TEST_P(BlendIntegerTest, RG16UI)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_RG16UI, 2, false>();
}

TEST_P(BlendIntegerTest, RG32UI)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_RG32UI, 2, false>();
}

TEST_P(BlendIntegerTest, RGBA8UI)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_RGBA8UI, 4, false>();
}

TEST_P(BlendIntegerTest, RGBA16UI)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_RGBA16UI, 4, false>();
}

TEST_P(BlendIntegerTest, RGBA32UI)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_RGBA32UI, 4, false>();
}

TEST_P(BlendIntegerTest, RGB10_A2UI)
{
    // TODO(http://anglebug.com/4571)
    ANGLE_SKIP_TEST_IF(isVulkanRenderer());
    runTest<GL_RGB10_A2UI, 4, false>();
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES3(BlendIntegerTest);
