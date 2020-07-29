//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"

using namespace angle;

class LineLoopTest : public ANGLETest
{
  protected:
    LineLoopTest()
    {
        setWindowWidth(256);
        setWindowHeight(256);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testSetUp() override
    {
        mProgram = CompileProgram(essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        mPositionLocation = glGetAttribLocation(mProgram, essl1_shaders::PositionAttrib());
        mColorLocation    = glGetUniformLocation(mProgram, essl1_shaders::ColorUniform());

        glBlendFunc(GL_ONE, GL_ONE);
        glEnable(GL_BLEND);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override { glDeleteProgram(mProgram); }

    void checkPixels()
    {
        std::vector<GLubyte> pixels(getWindowWidth() * getWindowHeight() * 4);
        glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                     &pixels[0]);

        ASSERT_GL_NO_ERROR();

        for (int y = 0; y < getWindowHeight(); y++)
        {
            for (int x = 0; x < getWindowWidth(); x++)
            {
                const GLubyte *pixel = &pixels[0] + ((y * getWindowWidth() + x) * 4);

                EXPECT_EQ(pixel[0], 0);
                EXPECT_EQ(pixel[1], pixel[2]);
                ASSERT_EQ(pixel[3], 255);
            }
        }
    }

    void runTest(GLenum indexType, GLuint indexBuffer, const void *indexPtr)
    {
        glClear(GL_COLOR_BUFFER_BIT);

        static const GLfloat loopPositions[] = {0.0f,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  0.0f,
                                                0.0f,  0.0f, 0.0f, 0.0f, 0.0f, -0.5f, -0.5f,
                                                -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f};

        static const GLfloat stripPositions[] = {-0.5f, -0.5f, -0.5f, 0.5f,
                                                 0.5f,  0.5f,  0.5f,  -0.5f};
        static const GLubyte stripIndices[]   = {1, 0, 3, 2, 1};

        glUseProgram(mProgram);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        glEnableVertexAttribArray(mPositionLocation);
        glVertexAttribPointer(mPositionLocation, 2, GL_FLOAT, GL_FALSE, 0, loopPositions);
        glUniform4f(mColorLocation, 0.0f, 0.0f, 1.0f, 1.0f);
        glDrawElements(GL_LINE_LOOP, 4, indexType, indexPtr);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glVertexAttribPointer(mPositionLocation, 2, GL_FLOAT, GL_FALSE, 0, stripPositions);
        glUniform4f(mColorLocation, 0, 1, 0, 1);
        glDrawElements(GL_LINE_STRIP, 5, GL_UNSIGNED_BYTE, stripIndices);

        checkPixels();
    }

    GLuint mProgram;
    GLint mPositionLocation;
    GLint mColorLocation;
};

TEST_P(LineLoopTest, LineLoopUByteIndices)
{
    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    // On Win7, the D3D SDK Layers emits a false warning for these tests.
    // This doesn't occur on Windows 10 (Version 1511) though.
    ignoreD3D11SDKLayersWarnings();

    static const GLubyte indices[] = {0, 7, 6, 9, 8, 0};
    runTest(GL_UNSIGNED_BYTE, 0, indices + 1);
}

TEST_P(LineLoopTest, LineLoopUShortIndices)
{
    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLushort indices[] = {0, 7, 6, 9, 8, 0};
    runTest(GL_UNSIGNED_SHORT, 0, indices + 1);
}

TEST_P(LineLoopTest, LineLoopUIntIndices)
{
    if (!IsGLExtensionEnabled("GL_OES_element_index_uint"))
    {
        return;
    }

    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLuint indices[] = {0, 7, 6, 9, 8, 0};
    runTest(GL_UNSIGNED_INT, 0, indices + 1);
}

TEST_P(LineLoopTest, LineLoopUByteIndexBuffer)
{
    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLubyte indices[] = {0, 7, 6, 9, 8, 0};

    GLuint buf;
    glGenBuffers(1, &buf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    runTest(GL_UNSIGNED_BYTE, buf, reinterpret_cast<const void *>(sizeof(GLubyte)));

    glDeleteBuffers(1, &buf);
}

TEST_P(LineLoopTest, LineLoopUShortIndexBuffer)
{
    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLushort indices[] = {0, 7, 6, 9, 8, 0};

    GLuint buf;
    glGenBuffers(1, &buf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    runTest(GL_UNSIGNED_SHORT, buf, reinterpret_cast<const void *>(sizeof(GLushort)));

    glDeleteBuffers(1, &buf);
}

TEST_P(LineLoopTest, LineLoopUIntIndexBuffer)
{
    if (!IsGLExtensionEnabled("GL_OES_element_index_uint"))
    {
        return;
    }

    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLuint indices[] = {0, 7, 6, 9, 8, 0};

    GLuint buf;
    glGenBuffers(1, &buf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    runTest(GL_UNSIGNED_INT, buf, reinterpret_cast<const void *>(sizeof(GLuint)));

    glDeleteBuffers(1, &buf);
}

// Tests an edge case with a very large line loop element count.
// Disabled because it is slow and triggers an internal error.
TEST_P(LineLoopTest, DISABLED_DrawArraysWithLargeCount)
{
    constexpr char kVS[] = "void main() { gl_Position = vec4(0); }";
    constexpr char kFS[] = "void main() { gl_FragColor = vec4(0, 1, 0, 1); }";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);
    glDrawArrays(GL_LINE_LOOP, 0, 0x3FFFFFFE);
    EXPECT_GL_ERROR(GL_OUT_OF_MEMORY);

    glDrawArrays(GL_LINE_LOOP, 0, 0x1FFFFFFE);
    EXPECT_GL_NO_ERROR();
}

class LineLoopIndirectTest : public LineLoopTest
{
  protected:
    void runTest(GLenum indexType, const void *indices, GLuint indicesSize, GLuint firstIndex)
    {
        struct DrawCommand
        {
            GLuint count;
            GLuint primCount;
            GLuint firstIndex;
            GLint baseVertex;
            GLuint reservedMustBeZero;
        };

        glClear(GL_COLOR_BUFFER_BIT);

        static const GLfloat loopPositions[] = {0.0f,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  0.0f,
                                                0.0f,  0.0f, 0.0f, 0.0f, 0.0f, -0.5f, -0.5f,
                                                -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f};

        static const GLfloat stripPositions[] = {-0.5f, -0.5f, -0.5f, 0.5f,
                                                 0.5f,  0.5f,  0.5f,  -0.5f};
        static const GLubyte stripIndices[]   = {1, 0, 3, 2, 1};

        glUseProgram(mProgram);

        GLuint vertexArray = 0;
        glGenVertexArrays(1, &vertexArray);
        glBindVertexArray(vertexArray);

        ASSERT_GL_NO_ERROR();

        GLuint vertBuffer = 0;
        glGenBuffers(1, &vertBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(loopPositions), loopPositions, GL_STATIC_DRAW);
        glEnableVertexAttribArray(mPositionLocation);
        glVertexAttribPointer(mPositionLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glUniform4f(mColorLocation, 0.0f, 0.0f, 1.0f, 1.0f);

        ASSERT_GL_NO_ERROR();

        GLuint buf;
        glGenBuffers(1, &buf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesSize, indices, GL_STATIC_DRAW);

        DrawCommand cmdBuffer = {};
        cmdBuffer.count       = 4;
        cmdBuffer.firstIndex  = firstIndex;
        cmdBuffer.primCount   = 1;
        GLuint indirectBuf    = 0;
        glGenBuffers(1, &indirectBuf);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuf);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawCommand), &cmdBuffer, GL_STATIC_DRAW);

        ASSERT_GL_NO_ERROR();

        glDrawElementsIndirect(GL_LINE_LOOP, indexType, nullptr);
        ASSERT_GL_NO_ERROR();

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
        glDeleteBuffers(1, &indirectBuf);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &buf);

        glBindVertexArray(0);
        glDeleteVertexArrays(1, &vertexArray);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &vertBuffer);

        glEnableVertexAttribArray(mPositionLocation);
        glVertexAttribPointer(mPositionLocation, 2, GL_FLOAT, GL_FALSE, 0, stripPositions);
        glUniform4f(mColorLocation, 0, 1, 0, 1);
        glDrawElements(GL_LINE_STRIP, 5, GL_UNSIGNED_BYTE, stripIndices);

        checkPixels();
    }
};

TEST_P(LineLoopIndirectTest, UByteIndexIndirectBuffer)
{
    // Old drivers buggy with optimized ConvertIndexIndirectLineLoop shader.
    // http://anglebug.com/4720
    ANGLE_SKIP_TEST_IF(IsAMD() && IsWindows() && IsVulkan());

    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLubyte indices[] = {0, 7, 6, 9, 8, 0};

    // Start at index 1.
    runTest(GL_UNSIGNED_BYTE, reinterpret_cast<const void *>(indices), sizeof(indices), 1);
}

TEST_P(LineLoopIndirectTest, UShortIndexIndirectBuffer)
{
    // Old drivers buggy with optimized ConvertIndexIndirectLineLoop shader.
    // http://anglebug.com/4720
    ANGLE_SKIP_TEST_IF(IsAMD() && IsWindows() && IsVulkan());

    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLushort indices[] = {0, 7, 6, 9, 8, 0};

    // Start at index 1.
    runTest(GL_UNSIGNED_SHORT, reinterpret_cast<const void *>(indices), sizeof(indices), 1);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2(LineLoopTest);

ANGLE_INSTANTIATE_TEST_ES31(LineLoopIndirectTest);
