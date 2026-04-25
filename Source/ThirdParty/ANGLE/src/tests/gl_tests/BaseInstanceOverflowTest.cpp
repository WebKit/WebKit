//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BaseInstanceOverflowTest: Reproduces integer overflow in vertex buffer streaming path.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

class BaseInstanceOverflowTest : public ANGLETest<>
{
  protected:
    BaseInstanceOverflowTest()
    {
        setWindowWidth(1);
        setWindowHeight(1);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Regression Test (crbug.com/489791424)
// Reproduces integer overflow in vertex buffer streaming path.
// The bug occurs when baseInstance is large, causing a wrap-around in reservation
// while the copy path uses 64-bit math.
TEST_P(BaseInstanceOverflowTest, BaseInstanceOverflow)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_base_vertex_base_instance"));

    // We need a dynamic vertex attribute to trigger the streaming path.
    // In D3D11, using an unaligned offset forces the streaming path.
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    std::vector<GLfloat> data(1000, 1.0f);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(GLfloat), data.data(), GL_DYNAMIC_DRAW);

    GLProgram program;
    program.makeRaster(essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    glUseProgram(program);

    GLint posLoc = glGetAttribLocation(program, essl1_shaders::PositionAttrib());
    glEnableVertexAttribArray(posLoc);

    // Unaligned offset (1 byte) forces streaming path in D3D11.
    glVertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 16, reinterpret_cast<void *>(1));
    glVertexAttribDivisor(posLoc, 1);

    // Trigger overflow by using a large baseInstance.
    // elementCount calculation in Renderer11::getVertexSpaceRequired:
    // (instances + baseInstance) / divisor
    // If instances = 300, baseInstance = 0xFFFFFF00, divisor = 1:
    // 300 + 0xFFFFFF00 = 0x10000002C.
    // 32-bit truncation results in 0x2C = 44 elements reserved.
    // However, StreamingVertexBufferInterface::storeDynamicAttribute uses 64-bit size_t:
    // adjustedCount = 300 + 0xFFFFFF00 = 4,294,967,340 elements copied.
    GLuint baseInstance   = 0xFFFFFF00;
    GLsizei instanceCount = 300;

    // This call is expected to crash the GPU process without the fix.
    // With the fix, the large baseInstance is caught by validation in VertexDataManager (D3D11)
    // or ContextGL (OpenGL) because it exceeds the source buffer's bounds, returning
    // GL_INVALID_OPERATION (D3D11) or avoiding the draw (OpenGL fallback).
    glDrawArraysInstancedBaseInstanceANGLE(GL_TRIANGLES, 0, 3, instanceCount, baseInstance);

    if (isD3D11Renderer())
    {
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
    else
    {
        EXPECT_GL_NO_ERROR();
    }
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BaseInstanceOverflowTest);
ANGLE_INSTANTIATE_TEST(BaseInstanceOverflowTest,
                       ES3_D3D11().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
                       ES3_OPENGL().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
                       ES3_OPENGLES().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
                       ES3_VULKAN().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
                       ES3_METAL().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions));
