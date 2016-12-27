//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BuiltinVariableTest:
//   Tests the correctness of the builtin GLSL variables.
//

#include "test_utils/ANGLETest.h"

using namespace angle;

class BuiltinVariableVertexIdTest : public ANGLETest
{
  protected:
    BuiltinVariableVertexIdTest()
    {
        setWindowWidth(64);
        setWindowHeight(64);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        const std::string vsSource =
            "#version 300 es\n"
            "precision highp float;\n"
            "in vec4 position;\n"
            "in int expectedID;"
            "out vec4 color;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    gl_Position = position;\n"
            "    color = vec4(gl_VertexID != expectedID, gl_VertexID == expectedID, 0.0, 1.0);"
            "}\n";

        const std::string fsSource =
            "#version 300 es\n"
            "precision highp float;\n"
            "in vec4 color;\n"
            "out vec4 fragColor;\n"
            "void main()\n"
            "{\n"
            "    fragColor = color;\n"
            "}\n";

        mProgram = CompileProgram(vsSource, fsSource);
        ASSERT_NE(0u, mProgram);

        mPositionLocation = glGetAttribLocation(mProgram, "position");
        ASSERT_NE(-1, mPositionLocation);
        mExpectedIdLocation = glGetAttribLocation(mProgram, "expectedID");
        ASSERT_NE(-1, mExpectedIdLocation);

        static const float positions[] =
        {
             0.5,  0.5,
            -0.5,  0.5,
             0.5, -0.5,
            -0.5, -0.5
        };
        glGenBuffers(1, &mPositionBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, mPositionBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);

        ASSERT_GL_NO_ERROR();
    }

    void TearDown() override
    {
        glDeleteBuffers(1, &mPositionBuffer);
        glDeleteBuffers(1, &mExpectedIdBuffer);
        glDeleteBuffers(1, &mIndexBuffer);
        glDeleteProgram(mProgram);

        ANGLETest::TearDown();
    }

    // Renders a primitive using the specified mode, each vertex color will
    // be green if gl_VertexID is correct, red otherwise.
    void runTest(GLuint drawMode, const std::vector<GLint> &indices, int count)
    {
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        glGenBuffers(1, &mIndexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLint) * indices.size(), indices.data(),
                     GL_STATIC_DRAW);

        std::vector<GLint> expectedIds = makeRange(count);

        glGenBuffers(1, &mExpectedIdBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, mExpectedIdBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLint) * expectedIds.size(), expectedIds.data(),
                     GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, mPositionBuffer);
        glVertexAttribPointer(mPositionLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(mPositionLocation);

        glBindBuffer(GL_ARRAY_BUFFER, mExpectedIdBuffer);
        glVertexAttribIPointer(mExpectedIdLocation, 1, GL_INT, 0, 0);
        glEnableVertexAttribArray(mExpectedIdLocation);

        glUseProgram(mProgram);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);
        glDrawElements(drawMode, count, GL_UNSIGNED_INT, 0);

        std::vector<GLColor> pixels(getWindowWidth() * getWindowHeight());
        glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                     pixels.data());

        ASSERT_GL_NO_ERROR();

        const GLColor green(0, 255, 0, 255);
        const GLColor black(0, 0, 0, 255);

        for (const auto &pixel : pixels)
        {
            EXPECT_TRUE(pixel == green || pixel == black);
        }
    }

    std::vector<GLint> makeRange(int n) const
    {
        std::vector<GLint> result;
        for (int i = 0; i < n; i++)
        {
            result.push_back(i);
        }

        return result;
    }

    GLuint mPositionBuffer   = 0;
    GLuint mExpectedIdBuffer = 0;
    GLuint mIndexBuffer      = 0;

    GLuint mProgram           = 0;
    GLint mPositionLocation   = -1;
    GLint mExpectedIdLocation = -1;
};

// Test gl_VertexID when rendering points
TEST_P(BuiltinVariableVertexIdTest, Points)
{
    runTest(GL_POINTS, makeRange(4), 4);
}

// Test gl_VertexID when rendering line strips
TEST_P(BuiltinVariableVertexIdTest, LineStrip)
{
    runTest(GL_LINE_STRIP, makeRange(4), 4);
}

// Test gl_VertexID when rendering line loops
TEST_P(BuiltinVariableVertexIdTest, LineLoop)
{
    runTest(GL_LINE_LOOP, makeRange(4), 4);
}

// Test gl_VertexID when rendering lines
TEST_P(BuiltinVariableVertexIdTest, Lines)
{
    runTest(GL_LINES, makeRange(4), 4);
}

// Test gl_VertexID when rendering triangle strips
TEST_P(BuiltinVariableVertexIdTest, TriangleStrip)
{
    runTest(GL_TRIANGLE_STRIP, makeRange(4), 4);
}

// Test gl_VertexID when rendering triangle fans
TEST_P(BuiltinVariableVertexIdTest, TriangleFan)
{
    std::vector<GLint> indices;
    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(3);
    indices.push_back(2);
    runTest(GL_TRIANGLE_FAN, indices, 4);
}

// Test gl_VertexID when rendering triangles
TEST_P(BuiltinVariableVertexIdTest, Triangles)
{
    std::vector<GLint> indices;
    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(2);
    indices.push_back(1);
    indices.push_back(2);
    indices.push_back(3);
    runTest(GL_TRIANGLES, indices, 6);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(BuiltinVariableVertexIdTest, ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES());
