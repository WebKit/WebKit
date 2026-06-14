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
    else if (isMetalRenderer())
    {
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
    else
    {
        EXPECT_GL_NO_ERROR();
    }
}

// Test that baseinstance + primcount overflow is properly validated for backends that cannot draw
// large baseInstance + primcount.
TEST_P(BaseInstanceOverflowTest, BaseInstanceOverflow2)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_base_vertex_base_instance"));

    constexpr char kVS[] = R"(
        attribute vec4 a_position;
        attribute float a_instanceData;
        varying float v_data;
        void main() {
            gl_Position = a_position;
            v_data = a_instanceData;
        })";

    constexpr char kFS[] = R"(
        precision mediump float;
        varying float v_data;
        void main() {
            gl_FragColor = vec4(0.0, v_data, 0.0, 1.0);
        })";

    GLProgram program;
    program.makeRaster(kVS, kFS);
    ASSERT_TRUE(program.valid());
    glUseProgram(program);

    GLint posLoc  = glGetAttribLocation(program, "a_position");
    GLint dataLoc = glGetAttribLocation(program, "a_instanceData");
    ASSERT_NE(posLoc, -1);
    ASSERT_NE(dataLoc, -1);

    // Non-instanced position buffer: full-screen triangle.
    // clang-format off
    const GLfloat kQuadVerts[] = {
        -1.0f, -1.0f,
         3.0f, -1.0f,
        -1.0f,  3.0f,
    };
    // clang-format on
    GLBuffer posBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, posBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Instanced attribute setup.
    // Use a large divisor so that all instances read the same element:
    //   element index = baseInstance + floor(i / divisor)
    // With divisor >= primcount, floor(i / divisor) == 0 for all i in [0, primcount-1].
    // So only element at index baseInstance is accessed.
    glEnableVertexAttribArray(dataLoc);

    struct Subcase
    {
        GLsizei primcount;
        GLuint baseInstance;
    };

    // Three subcases:
    //   small:  5 + 100 = 105, well within GLuint range -> should pass
    //   medium: 0x7FFFFFFF + 1000 = 0x800003E7, within GLuint range -> should pass
    //   large:  0xFFFFFF00 + 300 = overflow past GLuint max -> should fail
    Subcase subcases[] = {
        {100, 5},
        {1000, 0x7FFFFFFFu},
        {300, 0xFFFFFF00u},
    };

    for (const auto &subcase : subcases)
    {
        SCOPED_TRACE(testing::Message() << "primcount: " << subcase.primcount
                                        << " baseInstance:" << subcase.baseInstance);
        // Use divisor = primcount so only one element (at baseInstance) is accessed.
        const GLuint divisor = static_cast<GLuint>(subcase.primcount);
        glVertexAttribDivisor(dataLoc, divisor);

        // Allocate attribute buffer large enough for the one element at baseInstance.
        // Size needed: (baseInstance + 1) * sizeof(GLfloat).
        GLsizeiptr bufSize = (static_cast<GLsizeiptr>(subcase.baseInstance) + 1) * sizeof(GLfloat);

        GLBuffer instBuffer;
        glBindBuffer(GL_ARRAY_BUFFER, instBuffer);
        glBufferData(GL_ARRAY_BUFFER, bufSize, nullptr, GL_DYNAMIC_DRAW);
        ANGLE_SKIP_TEST_IF(glGetError() == GL_OUT_OF_MEMORY);

        // Write 1.0f at element index baseInstance.
        GLfloat one = 1.0f;
        glBufferSubData(GL_ARRAY_BUFFER, subcase.baseInstance * sizeof(GLfloat), sizeof(GLfloat),
                        &one);
        ASSERT_GL_NO_ERROR();

        glVertexAttribPointer(dataLoc, 1, GL_FLOAT, GL_FALSE, sizeof(GLfloat), nullptr);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArraysInstancedBaseInstanceANGLE(GL_TRIANGLES, 0, 3, subcase.primcount,
                                               subcase.baseInstance);

        const bool expectOverflow =
            static_cast<uint64_t>(subcase.primcount - 1) + subcase.baseInstance >
            std::numeric_limits<GLuint>::max();
        GLenum error = glGetError();
        if (error == GL_INVALID_OPERATION)
        {
            EXPECT_TRUE(expectOverflow);
        }
        else
        {
            EXPECT_EQ(error, static_cast<GLenum>(GL_NO_ERROR));
            EXPECT_TRUE(!(isMetalRenderer() && expectOverflow));

            // No overflow; the draw should succeed and produce green.
            ASSERT_GL_NO_ERROR();
            EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
        }
    }
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BaseInstanceOverflowTest);
ANGLE_INSTANTIATE_TEST(BaseInstanceOverflowTest,
                       ES3_D3D11().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
                       ES3_OPENGL().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
                       ES3_OPENGLES().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
                       ES3_VULKAN().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),
                       ES3_METAL().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions));
