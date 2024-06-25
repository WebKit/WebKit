//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"

using namespace angle;

class LineLoopTest : public ANGLETest<>
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

                EXPECT_EQ(pixel[0], 0) << "Failed at " << x << ", " << y << std::endl;
                EXPECT_EQ(pixel[1], pixel[2]) << "Failed at " << x << ", " << y << std::endl;
                ASSERT_EQ(pixel[3], 255) << "Failed at " << x << ", " << y << std::endl;
            }
        }
    }

    void preTestUpdateBuffer(GLuint framebuffer, GLuint texture, GLuint buffer, GLsizei size)
    {
        GLsizei uboSize = std::max(size, 16);
        const std::vector<uint32_t> initialData((uboSize + 3) / 4, 0x1234567u);

        glBindTexture(GL_TEXTURE_2D, texture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture,
                               0);

        glBindBuffer(GL_UNIFORM_BUFFER, buffer);
        glBufferData(GL_UNIFORM_BUFFER, uboSize, initialData.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, buffer);

        constexpr char kVerifyUBO[] = R"(#version 300 es
precision mediump float;
uniform block {
    uint data;
} ubo;
out vec4 colorOut;
void main()
{
    if (ubo.data == 0x1234567u)
        colorOut = vec4(0, 1.0, 0, 1.0);
    else
        colorOut = vec4(1.0, 0, 0, 1.0);
})";

        ANGLE_GL_PROGRAM(verifyUbo, essl3_shaders::vs::Simple(), kVerifyUBO);

        glDisable(GL_BLEND);
        drawQuad(verifyUbo, essl3_shaders::PositionAttrib(), 0.5);

        EXPECT_GL_NO_ERROR();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

        glEnable(GL_BLEND);

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

    GLBuffer buf;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    runTest(GL_UNSIGNED_BYTE, buf, reinterpret_cast<const void *>(sizeof(GLubyte)));
}

TEST_P(LineLoopTest, LineLoopUShortIndexBuffer)
{
    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLushort indices[] = {0, 7, 6, 9, 8, 0};

    GLBuffer buf;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    runTest(GL_UNSIGNED_SHORT, buf, reinterpret_cast<const void *>(sizeof(GLushort)));
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

    GLBuffer buf;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    runTest(GL_UNSIGNED_INT, buf, reinterpret_cast<const void *>(sizeof(GLuint)));
}

class LineLoopTestES3 : public LineLoopTest
{};

// Test that uploading data to buffer that's in use then using it for line loop elements works.
TEST_P(LineLoopTestES3, UseAsUBOThenUpdateThenLineLoopUByteIndexBuffer)
{
    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLubyte indices[] = {0, 7, 6, 9, 8, 0};

    GLFramebuffer framebuffer;
    GLTexture texture;

    GLBuffer buf;

    preTestUpdateBuffer(framebuffer, texture, buf, sizeof(indices));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(indices), indices);

    runTest(GL_UNSIGNED_BYTE, buf, reinterpret_cast<const void *>(sizeof(GLubyte)));

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that uploading data to buffer that's in use then using it for line loop elements works.
TEST_P(LineLoopTestES3, UseAsUBOThenUpdateThenLineLoopUShortIndexBuffer)
{
    // http://anglebug.com/42264370
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsQualcomm());

    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLushort indices[] = {0, 7, 6, 9, 8, 0};

    GLFramebuffer framebuffer;
    GLTexture texture;

    GLBuffer buf;

    preTestUpdateBuffer(framebuffer, texture, buf, sizeof(indices));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(indices), indices);

    runTest(GL_UNSIGNED_SHORT, buf, reinterpret_cast<const void *>(sizeof(GLushort)));

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that uploading data to buffer that's in use then using it for line loop elements works.
TEST_P(LineLoopTestES3, UseAsUBOThenUpdateThenLineLoopUIntIndexBuffer)
{
    if (!IsGLExtensionEnabled("GL_OES_element_index_uint"))
    {
        return;
    }

    // http://anglebug.com/42264370
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsQualcomm());

    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLuint indices[] = {0, 7, 6, 9, 8, 0};

    GLFramebuffer framebuffer;
    GLTexture texture;

    GLBuffer buf;

    preTestUpdateBuffer(framebuffer, texture, buf, sizeof(indices));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(indices), indices);

    runTest(GL_UNSIGNED_INT, buf, reinterpret_cast<const void *>(sizeof(GLuint)));

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
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

class LineLoopPrimitiveRestartTest : public ANGLETest<>
{
  protected:
    LineLoopPrimitiveRestartTest()
    {
        setWindowWidth(64);
        setWindowHeight(64);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

TEST_P(LineLoopPrimitiveRestartTest, LineLoopWithPrimitiveRestart)
{
    constexpr char kVS[] = R"(#version 300 es
in vec2 a_position;
// x,y = offset, z = scale
in vec3 a_transform;

invariant gl_Position;
void main()
{
    vec2 v_position = a_transform.z * a_position + a_transform.xy;
    gl_Position = vec4(v_position, 0.0, 1.0);
})";

    constexpr char kFS[] = R"(#version 300 es
precision highp float;
layout (location=0) out vec4 fragColor;
void main()
{
    fragColor = vec4(1.0, 0.0, 0.0, 1.0);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1, "a_transform");
    glLinkProgram(program);
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    // clang-format off
    constexpr GLfloat vertices[] = {
        0.1, 0.1, -0.1, 0.1, -0.1, -0.1, 0.1, -0.1,
        0.1, 0.1, -0.1, 0.1, -0.1, -0.1, 0.1, -0.1,
        0.1, 0.1, -0.1, 0.1, -0.1, -0.1, 0.1, -0.1,
        0.1, 0.1, -0.1, 0.1, -0.1, -0.1, 0.1, -0.1,
    };

    constexpr GLfloat transform[] = {
        // first loop transform
        0, 0, 9,
        0, 0, 9,
        0, 0, 9,
        0, 0, 9,
        // second loop transform
        0.2, 0.1, 2,
        0.2, 0.1, 2,
        0.2, 0.1, 2,
        0.2, 0.1, 2,
        // third loop transform
        0.5, -0.2, 3,
        0.5, -0.2, 3,
        0.5, -0.2, 3,
        0.5, -0.2, 3,
        // forth loop transform
        -0.8, -0.5, 1,
        -0.8, -0.5, 1,
        -0.8, -0.5, 1,
        -0.8, -0.5, 1,
    };

    constexpr GLushort lineloopAsStripIndices[] = {
        // first strip
        0, 1, 2, 3, 0,
        // second strip
        4, 5, 6, 7, 4,
        // third strip
        8, 9, 10, 11, 8,
        // forth strip
        12, 13, 14, 15, 12 };

    constexpr GLushort lineloopWithRestartIndices[] = {
        // first loop
        0, 1, 2, 3, 0xffff,
        // second loop
        4, 5, 6, 7, 0xffff,
        // third loop
        8, 9, 10, 11, 0xffff,
        // forth loop
        12, 13, 14, 15,
    };
    // clang-format on

    std::vector<GLColor> expectedPixels(getWindowWidth() * getWindowHeight());
    std::vector<GLColor> renderedPixels(getWindowWidth() * getWindowHeight());

    // Draw in non-primitive restart way
    glClear(GL_COLOR_BUFFER_BIT);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    for (int loop = 0; loop < 4; ++loop)
    {
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices + 8 * loop);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, transform + 12 * loop);

        glDrawElements(GL_LINE_STRIP, 5, GL_UNSIGNED_SHORT, lineloopAsStripIndices);
    }

    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 expectedPixels.data());
    ASSERT_GL_NO_ERROR();

    // Draw line loop with primitive restart:
    glClear(GL_COLOR_BUFFER_BIT);

    GLBuffer vertexBuffer[2];
    GLBuffer indexBuffer;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(lineloopWithRestartIndices),
                 lineloopWithRestartIndices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(transform), transform, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawElements(GL_LINE_LOOP, ArraySize(lineloopWithRestartIndices), GL_UNSIGNED_SHORT, 0);

    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 renderedPixels.data());

    for (int y = 0; y < getWindowHeight(); ++y)
    {
        for (int x = 0; x < getWindowWidth(); ++x)
        {
            int idx = y * getWindowWidth() + x;
            EXPECT_EQ(expectedPixels[idx], renderedPixels[idx])
                << "Expected pixel at " << x << ", " << y << " to be " << expectedPixels[idx]
                << std::endl;
        }
    }
}

class LineLoopIndirectTest : public LineLoopTest
{
  protected:
    void runTest(GLenum indexType,
                 const void *indices,
                 GLuint indicesSize,
                 GLuint firstIndex,
                 bool useBuffersAsUboFirst)
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

        GLVertexArray vertexArray;

        ASSERT_GL_NO_ERROR();

        GLFramebuffer arrayUpdateFbo, elementUpdateFbo;
        GLTexture arrayUpdateTex, elementUpdateTex;

        GLBuffer vertBuffer;
        GLBuffer buf;

        if (useBuffersAsUboFirst)
        {
            preTestUpdateBuffer(arrayUpdateFbo, arrayUpdateTex, vertBuffer, sizeof(loopPositions));
            preTestUpdateBuffer(elementUpdateFbo, elementUpdateTex, buf, indicesSize);
        }

        glBindVertexArray(vertexArray);
        glBindBuffer(GL_ARRAY_BUFFER, vertBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf);

        if (useBuffersAsUboFirst)
        {
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(loopPositions), loopPositions);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indicesSize, indices);
        }
        else
        {
            glBufferData(GL_ARRAY_BUFFER, sizeof(loopPositions), loopPositions, GL_STATIC_DRAW);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesSize, indices, GL_STATIC_DRAW);
        }
        glEnableVertexAttribArray(mPositionLocation);
        glVertexAttribPointer(mPositionLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glUniform4f(mColorLocation, 0.0f, 0.0f, 1.0f, 1.0f);

        ASSERT_GL_NO_ERROR();

        DrawCommand cmdBuffer = {};
        cmdBuffer.count       = 4;
        cmdBuffer.firstIndex  = firstIndex;
        cmdBuffer.primCount   = 1;
        GLBuffer indirectBuf;
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuf);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawCommand), &cmdBuffer, GL_STATIC_DRAW);

        ASSERT_GL_NO_ERROR();

        glEnable(GL_BLEND);
        glDrawElementsIndirect(GL_LINE_LOOP, indexType, nullptr);
        ASSERT_GL_NO_ERROR();

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glBindVertexArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(mPositionLocation);
        glVertexAttribPointer(mPositionLocation, 2, GL_FLOAT, GL_FALSE, 0, stripPositions);
        glUniform4f(mColorLocation, 0, 1, 0, 1);
        glDrawElements(GL_LINE_STRIP, 5, GL_UNSIGNED_BYTE, stripIndices);

        checkPixels();

        if (useBuffersAsUboFirst)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, arrayUpdateFbo);
            EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

            glBindFramebuffer(GL_FRAMEBUFFER, elementUpdateFbo);
            EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
        }
    }
};

TEST_P(LineLoopIndirectTest, UByteIndexIndirectBuffer)
{
    // Old drivers buggy with optimized ConvertIndexIndirectLineLoop shader.
    // http://anglebug.com/40096699
    ANGLE_SKIP_TEST_IF(IsAMD() && IsWindows() && IsVulkan());

    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLubyte indices[] = {0, 7, 6, 9, 8, 0};

    // Start at index 1.
    runTest(GL_UNSIGNED_BYTE, reinterpret_cast<const void *>(indices), sizeof(indices), 1, false);
}

TEST_P(LineLoopIndirectTest, UShortIndexIndirectBuffer)
{
    // Old drivers buggy with optimized ConvertIndexIndirectLineLoop shader.
    // http://anglebug.com/40096699
    ANGLE_SKIP_TEST_IF(IsAMD() && IsWindows() && IsVulkan());

    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLushort indices[] = {0, 7, 6, 9, 8, 0};

    // Start at index 1.
    runTest(GL_UNSIGNED_SHORT, reinterpret_cast<const void *>(indices), sizeof(indices), 1, false);
}

// Test that uploading data to buffer that's in use then using it for line loop elements works.
TEST_P(LineLoopIndirectTest, UseAsUBOThenUpdateThenUByteIndexIndirectBuffer)
{
    // http://anglebug.com/42264370
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsQualcomm());

    // Old drivers buggy with optimized ConvertIndexIndirectLineLoop shader.
    // http://anglebug.com/40096699
    ANGLE_SKIP_TEST_IF(IsAMD() && IsWindows() && IsVulkan());

    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLubyte indices[] = {0, 7, 6, 9, 8, 0};

    // Start at index 1.
    runTest(GL_UNSIGNED_BYTE, reinterpret_cast<const void *>(indices), sizeof(indices), 1, true);
}

// Test that uploading data to buffer that's in use then using it for line loop elements works.
TEST_P(LineLoopIndirectTest, UseAsUBOThenUpdateThenUShortIndexIndirectBuffer)
{
    // http://anglebug.com/42264370
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsQualcomm());

    // Old drivers buggy with optimized ConvertIndexIndirectLineLoop shader.
    // http://anglebug.com/40096699
    ANGLE_SKIP_TEST_IF(IsAMD() && IsWindows() && IsVulkan());

    // Disable D3D11 SDK Layers warnings checks, see ANGLE issue 667 for details
    ignoreD3D11SDKLayersWarnings();

    static const GLushort indices[] = {0, 7, 6, 9, 8, 0};

    // Start at index 1.
    runTest(GL_UNSIGNED_SHORT, reinterpret_cast<const void *>(indices), sizeof(indices), 1, true);
}

ANGLE_INSTANTIATE_TEST_ES2(LineLoopTest);
ANGLE_INSTANTIATE_TEST_ES3(LineLoopTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(LineLoopPrimitiveRestartTest);
ANGLE_INSTANTIATE_TEST_ES3_AND(
    LineLoopPrimitiveRestartTest,
    ES3_METAL().enable(Feature::ForceBufferGPUStorage),
    ES3_METAL().disable(Feature::HasExplicitMemBarrier).disable(Feature::HasCheapRenderPass));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(LineLoopIndirectTest);
ANGLE_INSTANTIATE_TEST_ES31(LineLoopIndirectTest);
