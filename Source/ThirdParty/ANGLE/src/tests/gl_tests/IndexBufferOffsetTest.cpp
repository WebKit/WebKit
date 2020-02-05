//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// IndexBufferOffsetTest.cpp: Test glDrawElements with an offset and an index buffer

#include "test_utils/ANGLETest.h"
#include "util/test_utils.h"

using namespace angle;

class IndexBufferOffsetTest : public ANGLETest
{
  protected:
    IndexBufferOffsetTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testSetUp() override
    {
        constexpr char kVS[] =
            R"(precision highp float;
            attribute vec2 position;

            void main()
            {
                gl_Position = vec4(position, 0.0, 1.0);
            })";

        constexpr char kFS[] =
            R"(precision highp float;
            uniform vec4 color;

            void main()
            {
                gl_FragColor = color;
            })";

        mProgram = CompileProgram(kVS, kFS);
        ASSERT_NE(0u, mProgram);

        mColorUniformLocation      = glGetUniformLocation(mProgram, "color");
        mPositionAttributeLocation = glGetAttribLocation(mProgram, "position");

        const GLfloat vertices[] = {-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f};
        glGenBuffers(1, &mVertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);

        glGenBuffers(1, &mIndexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);
    }

    void testTearDown() override
    {
        glDeleteBuffers(1, &mVertexBuffer);
        glDeleteBuffers(1, &mIndexBuffer);
        glDeleteProgram(mProgram);
    }

    void runTest(GLenum type, int typeWidth, void *indexData)
    {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        GLuint nullIndexData[] = {0, 0, 0, 0, 0, 0};

        size_t indexDataWidth = 6 * typeWidth;

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * indexDataWidth, nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indexDataWidth, nullIndexData);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, indexDataWidth, indexDataWidth, indexData);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 2 * indexDataWidth, indexDataWidth, nullIndexData);

        glUseProgram(mProgram);

        glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
        glVertexAttribPointer(mPositionAttributeLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(mPositionAttributeLocation);

        glUniform4f(mColorUniformLocation, 1.0f, 0.0f, 0.0f, 1.0f);

        for (int i = 0; i < 16; i++)
        {
            glDrawElements(GL_TRIANGLES, 6, type, reinterpret_cast<void *>(indexDataWidth));
            EXPECT_PIXEL_COLOR_EQ(64, 64, GLColor::red);
        }

        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, indexDataWidth, indexDataWidth, nullIndexData);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 2 * indexDataWidth, indexDataWidth, indexData);

        glUniform4f(mColorUniformLocation, 0.0f, 1.0f, 0.0f, 1.0f);
        glDrawElements(GL_TRIANGLES, 6, type, reinterpret_cast<void *>(indexDataWidth * 2));
        EXPECT_PIXEL_COLOR_EQ(64, 64, GLColor::green);

        EXPECT_GL_NO_ERROR();
        swapBuffers();
    }

    GLuint mProgram;
    GLint mColorUniformLocation;
    GLint mPositionAttributeLocation;
    GLuint mVertexBuffer;
    GLuint mIndexBuffer;
};

// Test using an offset for an UInt8 index buffer
TEST_P(IndexBufferOffsetTest, UInt8Index)
{
    GLubyte indexData[] = {0, 1, 2, 1, 2, 3};
    runTest(GL_UNSIGNED_BYTE, 1, indexData);
}

// Test using an offset for an UInt16 index buffer
TEST_P(IndexBufferOffsetTest, UInt16Index)
{
    // TODO(jie.a.chen@intel.com): Re-enable the test once the driver fix is
    // available in public release.
    // http://anglebug.com/2663
    ANGLE_SKIP_TEST_IF(IsIntel() && IsVulkan());

    GLushort indexData[] = {0, 1, 2, 1, 2, 3};
    runTest(GL_UNSIGNED_SHORT, 2, indexData);
}

// Test using an offset for an UInt32 index buffer
TEST_P(IndexBufferOffsetTest, UInt32Index)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       !IsGLExtensionEnabled("GL_OES_element_index_uint"));

    GLuint indexData[] = {0, 1, 2, 1, 2, 3};
    runTest(GL_UNSIGNED_INT, 4, indexData);
}

// Uses index buffer offset and 2 drawElement calls one of the other, makes sure the second
// drawElement call will use the correct offset.
TEST_P(IndexBufferOffsetTest, DrawAtDifferentOffsets)
{
    GLushort indexData[] = {0, 1, 2, 1, 2, 3};
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    size_t indexDataWidth = 6 * sizeof(GLushort);

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexDataWidth, indexData, GL_DYNAMIC_DRAW);
    glUseProgram(mProgram);

    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
    glVertexAttribPointer(mPositionAttributeLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(mPositionAttributeLocation);

    glUniform4f(mColorUniformLocation, 1.0f, 0.0f, 0.0f, 1.0f);

    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, 0);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT,
                   reinterpret_cast<void *>(indexDataWidth / 2));

    // Check the upper left triangle
    EXPECT_PIXEL_COLOR_EQ(0, getWindowHeight() / 4, GLColor::red);

    // Check the down right triangle
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::red);

    EXPECT_GL_NO_ERROR();
}

// Uses index buffer offset and 2 drawElement calls one of the other with different counts,
// makes sure the second drawElement call will have its data available.
TEST_P(IndexBufferOffsetTest, DrawWithDifferentCountsSameOffset)
{
    GLubyte indexData[] = {99, 0, 1, 2, 1, 2, 3};
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    size_t indexDataWidth = 7 * sizeof(GLubyte);

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexDataWidth, indexData, GL_DYNAMIC_DRAW);
    glUseProgram(mProgram);

    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
    glVertexAttribPointer(mPositionAttributeLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(mPositionAttributeLocation);

    glUniform4f(mColorUniformLocation, 1.0f, 0.0f, 0.0f, 1.0f);

    // The first draw draws the first triangle, and the second draws a quad.
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, reinterpret_cast<void *>(1));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, reinterpret_cast<void *>(1));

    // Check the upper left triangle
    EXPECT_PIXEL_COLOR_EQ(0, getWindowHeight() / 4, GLColor::red);

    // Check the down right triangle
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::red);

    EXPECT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST(IndexBufferOffsetTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_METAL(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES(),
                       ES2_VULKAN());
