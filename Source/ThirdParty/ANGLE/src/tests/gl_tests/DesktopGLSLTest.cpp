//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"
#include "util/shader_utils.h"

namespace angle
{

class DesktopGLSLTest : public ANGLETest<>
{
  protected:
    DesktopGLSLTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Simple test case of compiling a desktop OpenGL shader to verify that the shader compiler
// initializes.
TEST_P(DesktopGLSLTest, BasicCompilation)
{
    constexpr char kVS[] = R"(#version 150
in vec4 position;

void main()
{
    gl_Position = position;
})";

    constexpr char kFS[] = R"(#version 150
out vec4 fragColor;

void main()
{
    fragColor = vec4(1.0, 0.5, 0.2, 1.0);
})";

    ANGLE_GL_PROGRAM(testProgram, kVS, kFS);
}

// Test calling sh::Compile with fragment shader source string
TEST_P(DesktopGLSLTest, DesktopGLString)
{
    constexpr char kFS[] =
        R"(#version 330
        void main()
        {
        })";

    ASSERT_NE(static_cast<unsigned int>(0), CompileShader(GL_FRAGMENT_SHADER, kFS));
}

// Test calling sh::Compile with core version
TEST_P(DesktopGLSLTest, FragmentShaderCoreVersion)
{
    constexpr char kFS[] =
        R"(#version 330 core
        void main()
        {
        })";

    ASSERT_NE(static_cast<unsigned int>(0), CompileShader(GL_FRAGMENT_SHADER, kFS));
}

// Implicit conversions in basic operations
TEST_P(DesktopGLSLTest, ImplicitConversionBasicOperation)
{
    constexpr char kFS[] =
        R"(#version 330 core
        void main()
        {
            //float a = 1 + 1.5;
            //float b = 1 - 1.5;
            //float c = 1 * 1.5;
            //float d = 1 / 1.5;
            //float e = 1.5 + 1;
            //float f = 1.5 - 1;
            float g = 1.5 * 1;
            //float h = 1.5 / 1;
        })";

    ASSERT_NE(static_cast<unsigned int>(0), CompileShader(GL_FRAGMENT_SHADER, kFS));
}

// Implicit conversions when assigning
TEST_P(DesktopGLSLTest, ImplicitConversionAssign)
{
    constexpr char kFS[] =
        R"(#version 330 core
        void main()
        {
            float a = 1;
            uint b = 2u;
            a = b;
            a += b;
            a -= b;
            a *= b;
            a /= b;
        })";

    ASSERT_NE(static_cast<unsigned int>(0), CompileShader(GL_FRAGMENT_SHADER, kFS));
}

// Implicit conversions for vectors
TEST_P(DesktopGLSLTest, ImplicitConversionVector)
{
    constexpr char kFS[] =
        R"(#version 330 core
        void main()
        {
            vec3 a;
            ivec3 b = ivec3(1, 1, 1);
            a = b;
        })";

    ASSERT_NE(static_cast<unsigned int>(0), CompileShader(GL_FRAGMENT_SHADER, kFS));
}

// Implicit conversions should not convert between ints and uints
TEST_P(DesktopGLSLTest, ImplicitConversionAssignFailed)
{
    constexpr char kFS[] =
        R"(#version 330 core
        void main()
        {
            int a = 1;
            uint b = 2;
            a = b;
        })";

    ASSERT_NE(static_cast<unsigned int>(0), CompileShader(GL_FRAGMENT_SHADER, kFS));
}

// GL shaders use implicit conversions between types
// Testing internal implicit conversions
TEST_P(DesktopGLSLTest, ImplicitConversionFunction)
{
    constexpr char kFS[] =
        R"(#version 330 core
        void main()
        {
            float cosTheta = clamp(0.5,0,1);
            float exp = pow(0.5,2);
        })";

    ASSERT_NE(static_cast<unsigned int>(0), CompileShader(GL_FRAGMENT_SHADER, kFS));
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DesktopGLSLTest);
ANGLE_INSTANTIATE_TEST_GL32_CORE(DesktopGLSLTest);

}  // namespace angle
