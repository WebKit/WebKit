//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SimpleOperationTest:
//   Basic GL commands such as linking a program, initializing a buffer, etc.
//

#include "common/unsafe_buffers.h"
#include "test_utils/ANGLETest.h"

#include <numeric>
#include <vector>

#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"
#include "util/random_utils.h"

using namespace angle;

namespace
{
constexpr char kBasicVertexShader[] =
    R"(attribute vec3 position;
void main()
{
    gl_Position = vec4(position, 1);
})";

constexpr char kGreenFragmentShader[] =
    R"(void main()
{
    gl_FragColor = vec4(0, 1, 0, 1);
})";

class SimpleOperationTest : public ANGLETest<>
{
  protected:
    SimpleOperationTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void verifyBuffer(const std::vector<uint8_t> &data, GLenum binding);

    template <typename T>
    void testDrawElementsLineLoopUsingClientSideMemory(GLenum indexType,
                                                       int windowWidth,
                                                       int windowHeight);
};

class SimpleOperationTest31 : public SimpleOperationTest
{};

void SimpleOperationTest::verifyBuffer(const std::vector<uint8_t> &data, GLenum binding)
{
    if (!IsGLExtensionEnabled("GL_EXT_map_buffer_range"))
    {
        return;
    }

    uint8_t *mapPointer =
        static_cast<uint8_t *>(glMapBufferRangeEXT(GL_ARRAY_BUFFER, 0, 1024, GL_MAP_READ_BIT));
    ASSERT_GL_NO_ERROR();

    std::vector<uint8_t> readbackData(data.size());
    ANGLE_UNSAFE_TODO(memcpy(readbackData.data(), mapPointer, data.size()));
    glUnmapBufferOES(GL_ARRAY_BUFFER);

    EXPECT_EQ(data, readbackData);
}

// Validates if culling rasterization states work. Simply draws a quad with
// cull face enabled and make sure we still render correctly.
TEST_P(SimpleOperationTest, CullFaceEnabledState)
{
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);
    glUseProgram(program);

    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_CULL_FACE);

    drawQuad(program, "position", 0.0f, 1.0f, true);

    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Validates if culling rasterization states work. Simply draws a quad with
// cull face enabled with cullface front and make sure the face have not been rendered.
TEST_P(SimpleOperationTest, CullFaceFrontEnabledState)
{
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);
    glUseProgram(program);

    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_CULL_FACE);

    // Should make the quad disappear since we draw it front facing.
    glCullFace(GL_FRONT);

    drawQuad(program, "position", 0.0f, 1.0f, true);

    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);
}

// Validates if blending render states work. Simply draws twice and verify the color have been
// added in the final output.
TEST_P(SimpleOperationTest, BlendingRenderState)
{
    // The precision when blending isn't perfect and some tests fail with a color of 254 instead
    // of 255 on the green component. This is why we need 0.51 green instead of .5
    constexpr char halfGreenFragmentShader[] =
        R"(void main()
{
    gl_FragColor = vec4(0, 0.51, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kBasicVertexShader, halfGreenFragmentShader);
    glUseProgram(program);

    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    auto vertices = GetQuadVertices();

    const GLint positionLocation = glGetAttribLocation(program, "position");
    ASSERT_NE(-1, positionLocation);

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glEnableVertexAttribArray(positionLocation);

    // Drawing a quad once will give 0.51 green, but if we enable blending
    // with additive function we should end up with full green of 1.0 with
    // a clamping func of 1.0.
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));

    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests getting the GL_BLEND_EQUATION integer
TEST_P(SimpleOperationTest, BlendEquationGetInteger)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    constexpr std::array<GLenum, 5> equations = {GL_FUNC_ADD, GL_FUNC_SUBTRACT,
                                                 GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX};

    for (GLenum equation : equations)
    {
        glBlendEquation(equation);

        GLint currentEquation;
        glGetIntegerv(GL_BLEND_EQUATION, &currentEquation);
        ASSERT_GL_NO_ERROR();

        EXPECT_EQ(currentEquation, static_cast<GLint>(equation));
    }
}

TEST_P(SimpleOperationTest, CompileVertexShader)
{
    GLuint shader = CompileShader(GL_VERTEX_SHADER, kBasicVertexShader);
    EXPECT_NE(shader, 0u);
    glDeleteShader(shader);

    ASSERT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, CompileFragmentShaderSingleVaryingInput)
{
    constexpr char kFS[] = R"(precision mediump float;
varying vec4 v_input;
void main()
{
    gl_FragColor = v_input;
})";

    GLuint shader = CompileShader(GL_FRAGMENT_SHADER, kFS);
    EXPECT_NE(shader, 0u);
    glDeleteShader(shader);

    ASSERT_GL_NO_ERROR();
}

// Covers a simple bug in Vulkan to do with dependencies between the Surface and the default
// Framebuffer.
TEST_P(SimpleOperationTest, ClearAndSwap)
{
    glClearColor(1.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    swapBuffers();

    // Can't check the pixel result after the swap, and checking the pixel result affects the
    // behaviour of the test on the Vulkan back-end, so don't bother checking correctness.
    ASSERT_GL_NO_ERROR();
    ASSERT_FALSE(getGLWindow()->hasError());
}

// Deleting shader zero is no-op.
TEST_P(SimpleOperationTest, DeleteShaderZero)
{
    glDeleteShader(0);
    EXPECT_GL_NO_ERROR();
}

// Deleting program zero is no-op.
TEST_P(SimpleOperationTest, DeleteProgramZero)
{
    glDeleteProgram(0);
    EXPECT_GL_NO_ERROR();
}

// Simple case of setting a scissor, enabled or disabled.
TEST_P(SimpleOperationTest, ScissorTest)
{
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);

    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(getWindowWidth() / 4, getWindowHeight() / 4, getWindowWidth() / 2,
              getWindowHeight() / 2);

    // Fill the whole screen with a quad.
    drawQuad(program, "position", 0.0f, 1.0f, true);

    ASSERT_GL_NO_ERROR();

    // Test outside the scissor test, pitch black.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);

    // Test inside, green of the fragment shader.
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::green);
}

TEST_P(SimpleOperationTest, LinkProgramShadersNoInputs)
{
    constexpr char kVS[] = "void main() { gl_Position = vec4(1.0, 1.0, 1.0, 1.0); }";
    constexpr char kFS[] = "void main() { gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0); }";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    ASSERT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, LinkProgramWithUniforms)
{
    constexpr char kVS[] = R"(void main()
{
    gl_Position = vec4(1.0, 1.0, 1.0, 1.0);
})";
    constexpr char kFS[] = R"(precision mediump float;
uniform vec4 u_input;
void main()
{
    gl_FragColor = u_input;
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    const GLint uniformLoc = glGetUniformLocation(program, "u_input");
    EXPECT_NE(-1, uniformLoc);

    ASSERT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, LinkProgramWithAttributes)
{
    constexpr char kVS[] = R"(attribute vec4 a_input;
void main()
{
    gl_Position = a_input;
})";

    ANGLE_GL_PROGRAM(program, kVS, kGreenFragmentShader);

    const GLint attribLoc = glGetAttribLocation(program, "a_input");
    EXPECT_NE(-1, attribLoc);

    ASSERT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, BufferDataWithData)
{
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer);

    std::vector<uint8_t> data(1024);
    FillVectorWithRandomUBytes(&data);
    glBufferData(GL_ARRAY_BUFFER, data.size(), &data[0], GL_STATIC_DRAW);

    verifyBuffer(data, GL_ARRAY_BUFFER);

    ASSERT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, BufferDataWithNoData)
{
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW);

    ASSERT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, BufferSubData)
{
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer);

    constexpr size_t bufferSize = 1024;
    std::vector<uint8_t> data(bufferSize);
    FillVectorWithRandomUBytes(&data);

    glBufferData(GL_ARRAY_BUFFER, bufferSize, nullptr, GL_STATIC_DRAW);

    constexpr size_t subDataCount = 16;
    constexpr size_t sliceSize    = bufferSize / subDataCount;
    for (size_t i = 0; i < subDataCount; i++)
    {
        size_t offset = i * sliceSize;
        glBufferSubData(GL_ARRAY_BUFFER, offset, sliceSize, &data[offset]);
    }

    verifyBuffer(data, GL_ARRAY_BUFFER);

    ASSERT_GL_NO_ERROR();
}

// Simple quad test.
TEST_P(SimpleOperationTest, DrawQuad)
{
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);

    drawQuad(program, "position", 0.5f, 1.0f, true);

    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Simple quad test with data in client memory, not vertex buffer.
TEST_P(SimpleOperationTest, DrawQuadFromClientMemory)
{
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);

    drawQuad(program, "position", 0.5f, 1.0f, false);

    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Simple double quad test.
TEST_P(SimpleOperationTest, DrawQuadTwice)
{
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);

    drawQuad(program, "position", 0.5f, 1.0f, true);
    drawQuad(program, "position", 0.5f, 1.0f, true);

    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Simple line test.
TEST_P(SimpleOperationTest, DrawLine)
{
    // We assume in the test the width and height are equal and we are tracing
    // the line from bottom left to top right. Verify that all pixels along that line
    // have been traced with green.
    ASSERT_EQ(getWindowWidth(), getWindowHeight());

    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);
    glUseProgram(program);

    std::vector<Vector3> vertices = {{-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};

    const GLint positionLocation = glGetAttribLocation(program, "position");
    ASSERT_NE(-1, positionLocation);

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(positionLocation);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));

    glDisableVertexAttribArray(positionLocation);

    ASSERT_GL_NO_ERROR();

    for (int x = 0; x < getWindowWidth(); x++)
    {
        EXPECT_PIXEL_COLOR_EQ(x, x, GLColor::green);
    }
}

// Simple line test that will use a very large offset in the vertex attributes.
TEST_P(SimpleOperationTest, DrawLineWithLargeAttribPointerOffset)
{
    // We assume in the test the width and height are equal and we are tracing
    // the line from bottom left to top right. Verify that all pixels along that line
    // have been traced with green.
    ASSERT_EQ(getWindowWidth(), getWindowHeight());

    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);
    glUseProgram(program);

    int kOffset = 3315;
    std::vector<Vector3> vertices(kOffset);
    Vector3 vector1{-1.0f, -1.0f, 0.0f};
    Vector3 vector2{1.0f, 1.0f, 0.0f};
    vertices.emplace_back(vector1);
    vertices.emplace_back(vector2);

    const GLint positionLocation = glGetAttribLocation(program, "position");
    ASSERT_NE(-1, positionLocation);

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0,
                          reinterpret_cast<const void *>(kOffset * sizeof(vertices[0])));
    glEnableVertexAttribArray(positionLocation);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_LINES, 0, 2);

    glDisableVertexAttribArray(positionLocation);

    ASSERT_GL_NO_ERROR();

    for (auto x = 0; x < getWindowWidth(); x++)
    {
        EXPECT_PIXEL_COLOR_EQ(x, x, GLColor::green);
    }
}

// Simple line strip test.
TEST_P(SimpleOperationTest, DrawLineStrip)
{
    // We assume in the test the width and height are equal and we are tracing
    // the line from bottom left to center, then from center to bottom right.
    // Verify that all pixels along these lines have been traced with green.
    ASSERT_EQ(getWindowWidth(), getWindowHeight());

    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);
    glUseProgram(program);

    auto vertices =
        std::vector<Vector3>{{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, -1.0f, 0.0f}};

    const GLint positionLocation = glGetAttribLocation(program, "position");
    ASSERT_NE(-1, positionLocation);

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glEnableVertexAttribArray(positionLocation);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(vertices.size()));

    ASSERT_GL_NO_ERROR();

    const auto centerX = getWindowWidth() / 2;
    const auto centerY = getWindowHeight() / 2;

    for (auto x = 0; x < centerX; x++)
    {
        EXPECT_PIXEL_COLOR_EQ(x, x, GLColor::green);
    }

    for (auto x = centerX, y = centerY - 1; x < getWindowWidth() && y >= 0; x++, y--)
    {
        EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::green);
    }
}

class TriangleFanDrawTest : public SimpleOperationTest
{
  protected:
    void testSetUp() override
    {
        // We assume in the test the width and height are equal and we are tracing
        // 2 triangles to cover half the surface like this:
        ASSERT_EQ(getWindowWidth(), getWindowHeight());

        mProgram.makeRaster(kBasicVertexShader, kGreenFragmentShader);
        ASSERT_TRUE(mProgram.valid());
        glUseProgram(mProgram);

        const GLint positionLocation = glGetAttribLocation(mProgram, "position");
        ASSERT_NE(-1, positionLocation);

        glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(mVertices[0]) * mVertices.size(), mVertices.data(),
                     GL_STATIC_DRAW);
        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(positionLocation);

        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void readPixels()
    {
        if (mReadPixels.empty())
        {
            mReadPixels.resize(getWindowWidth() * getWindowWidth());
        }

        glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                     mReadPixels.data());
        EXPECT_GL_NO_ERROR();
    }

    void verifyPixelAt(int x, int y, const GLColor &expected)
    {
        EXPECT_EQ(mReadPixels[y * getWindowWidth() + x], expected);
    }

    void verifyTriangles()
    {
        readPixels();

        // Check 4 lines accross de triangles to make sure we filled it.
        // Don't check every pixel as it would slow down our tests.
        for (auto x = 0; x < getWindowWidth(); x++)
        {
            verifyPixelAt(x, x, GLColor::green);
        }

        for (auto x = getWindowWidth() / 3, y = 0; x < getWindowWidth(); x++, y++)
        {
            verifyPixelAt(x, y, GLColor::green);
        }

        for (auto x = getWindowWidth() / 2, y = 0; x < getWindowWidth(); x++, y++)
        {
            verifyPixelAt(x, y, GLColor::green);
        }

        for (auto x = (getWindowWidth() / 4) * 3, y = 0; x < getWindowWidth(); x++, y++)
        {
            verifyPixelAt(x, y, GLColor::green);
        }

        // Area outside triangles
        for (auto x = 0; x < getWindowWidth() - 2; x++)
        {
            verifyPixelAt(x, x + 2, GLColor::red);
        }
    }

    const std::vector<Vector3> mVertices = {{0.0f, 0.0f, 0.0f},
                                            {-1.0f, -1.0f, 0.0f},
                                            {0.0f, -1.0f, 0.0f},
                                            {1.0f, -1.0f, 0.0f},
                                            {1.0f, 1.0f, 0.0f}};

    GLBuffer mVertexBuffer;
    GLProgram mProgram;

    std::vector<GLColor> mReadPixels;
};

// Simple triangle fans test.
TEST_P(TriangleFanDrawTest, DrawTriangleFan)
{
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(mVertices.size()));

    EXPECT_GL_NO_ERROR();

    verifyTriangles();
}

// Triangle fans test with index buffer.
TEST_P(TriangleFanDrawTest, DrawTriangleFanElements)
{
    std::vector<GLubyte> indices = {0, 1, 2, 3, 4};

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * indices.size(), indices.data(),
                 GL_STATIC_DRAW);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawElements(GL_TRIANGLE_FAN, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_BYTE, 0);

    EXPECT_GL_NO_ERROR();

    verifyTriangles();
}

// Triangle fans test with primitive restart index at the middle.
TEST_P(TriangleFanDrawTest, DrawTriangleFanPrimitiveRestartAtMiddle)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    std::vector<GLubyte> indices = {0, 1, 2, 3, 0xff, 0, 4, 3};

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * indices.size(), indices.data(),
                 GL_STATIC_DRAW);
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

    glDrawElements(GL_TRIANGLE_FAN, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_BYTE, 0);

    EXPECT_GL_NO_ERROR();

    verifyTriangles();
}

// Triangle fans test with primitive restart at begin.
TEST_P(TriangleFanDrawTest, DrawTriangleFanPrimitiveRestartAtBegin)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    // Primitive restart index is at middle, but we will use draw call which index offset=4.
    std::vector<GLubyte> indices = {0, 1, 2, 3, 0xff, 0, 4, 3};

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * indices.size(), indices.data(),
                 GL_STATIC_DRAW);
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

    glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_BYTE, 0);
    glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_BYTE,
                   reinterpret_cast<void *>(sizeof(indices[0]) * 4));

    EXPECT_GL_NO_ERROR();

    verifyTriangles();
}

// Triangle fans test with primitive restart at end.
TEST_P(TriangleFanDrawTest, DrawTriangleFanPrimitiveRestartAtEnd)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    std::vector<GLubyte> indices = {0, 1, 2, 3, 4, 0xff};

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * indices.size(), indices.data(),
                 GL_STATIC_DRAW);
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

    glDrawElements(GL_TRIANGLE_FAN, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_BYTE, 0);

    EXPECT_GL_NO_ERROR();

    verifyTriangles();
}

// Triangle fans test with primitive restart enabled, but no indexed draw.
TEST_P(TriangleFanDrawTest, DrawTriangleFanPrimitiveRestartNonIndexedDraw)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    std::vector<GLubyte> indices = {0, 1, 2, 3, 4};

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * indices.size(), indices.data(),
                 GL_STATIC_DRAW);
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 5);

    EXPECT_GL_NO_ERROR();

    verifyTriangles();
}

// Draws triangle fans with glDrawArrays and glDrawElements: with each index type, with the indices
// and the vertices in buffers and in client memory, and instanced. The fans cover the left half of
// the window. The vertices that the fans do not use, and the indices before the indices of the
// fans, are in the right half, so a fan that uses them draws into the right half.
class TriangleFanTest : public ANGLETest<>
{
  protected:
    TriangleFanTest()
    {
        setWindowWidth(16);
        setWindowHeight(16);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    enum class Source
    {
        Buffer,
        ClientMemory,
    };

    static constexpr GLuint kRestart = std::numeric_limits<GLuint>::max();

    // clang-format off
    static constexpr std::array<GLfloat, 14> kPositions = {
         1.0f, -1.0f,  // 0: the lower right corner of the window
        -1.0f, -1.0f,  // 1: the lower left corner of the left half
         1.0f,  1.0f,  // 2: the upper right corner of the window
         0.0f, -1.0f,  // 3: the lower right corner of the left half
        -0.5f,  0.0f,  // 4: the center of the left half
         0.0f,  1.0f,  // 5: the upper right corner of the left half
        -1.0f,  1.0f,  // 6: the upper left corner of the left half
    };
    // clang-format on

    // The fan around the center of the left half that covers it.
    static constexpr std::array<GLuint, 6> kFanIndices = {4, 1, 3, 5, 6, 1};
    // The indices before the indices of the fan.
    static constexpr std::array<GLuint, 3> kPaddingIndices = {0, 2, 0};

    void testSetUp() override
    {
        // In ES 3.0 contexts, instance 1 draws the fans translated to the right half.
        constexpr char kVS[]          = R"(attribute vec2 a_position;
void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
})";
        constexpr char kInstancedVS[] = R"(#version 300 es
in vec2 a_position;
void main()
{
    gl_Position = vec4(a_position.x + float(gl_InstanceID), a_position.y, 0.0, 1.0);
})";
        constexpr char kInstancedFS[] = R"(#version 300 es
precision mediump float;
out vec4 color;
void main()
{
    color = vec4(0.0, 1.0, 0.0, 1.0);
})";
        if (getClientMajorVersion() >= 3)
        {
            mProgram.makeRaster(kInstancedVS, kInstancedFS);
        }
        else
        {
            mProgram.makeRaster(kVS, essl1_shaders::fs::Green());
        }
        ASSERT_TRUE(mProgram.valid());
        glUseProgram(mProgram);
        mPositionLocation = glGetAttribLocation(mProgram, "a_position");
        ASSERT_NE(-1, mPositionLocation);
        glEnableVertexAttribArray(mPositionLocation);
    }

    std::vector<Source> sources() const
    {
        if (mClientArraysEnabled)
        {
            return {Source::Buffer, Source::ClientMemory};
        }
        return {Source::Buffer};
    }

    // Instance counts to draw with: 0 for the non-instanced draw calls.
    std::vector<GLsizei> instanceCounts() const
    {
        if (getClientMajorVersion() >= 3)
        {
            return {0, 1, 2};
        }
        return {0};
    }

    bool unsignedIntIndicesSupported()
    {
        if (getClientMajorVersion() >= 3)
        {
            return true;
        }
        if (IsGLExtensionRequestable("GL_OES_element_index_uint"))
        {
            glRequestExtensionANGLE("GL_OES_element_index_uint");
        }
        return IsGLExtensionEnabled("GL_OES_element_index_uint");
    }

    void setPositions(const std::vector<GLfloat> &positions, Source source)
    {
        // The data stays alive until the next call, as client memory is read at draw time.
        mPositions = positions;
        if (source == Source::Buffer)
        {
            glBindBuffer(GL_ARRAY_BUFFER, mPositionBuffer);
            glBufferData(GL_ARRAY_BUFFER, mPositions.size() * sizeof(GLfloat), mPositions.data(),
                         GL_STATIC_DRAW);
            glVertexAttribPointer(mPositionLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        }
        else
        {
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glVertexAttribPointer(mPositionLocation, 2, GL_FLOAT, GL_FALSE, 0, mPositions.data());
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // Returns the positions of the vertices with the given indices.
    static std::vector<GLfloat> GetPositions(const std::vector<GLuint> &indices)
    {
        std::vector<GLfloat> positions;
        for (GLuint index : indices)
        {
            positions.push_back(kPositions[2 * index]);
            positions.push_back(kPositions[2 * index + 1]);
        }
        return positions;
    }

    void clear()
    {
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Checks that the fans covered the left half, and with 2 instances also the right half.
    void checkFans(GLsizei instanceCount)
    {
        ASSERT_GL_NO_ERROR();
        const int halfWidth = getWindowWidth() / 2;
        EXPECT_PIXEL_RECT_EQ(0, 0, halfWidth, getWindowHeight(), GLColor::green);
        EXPECT_PIXEL_RECT_EQ(halfWidth, 0, getWindowWidth() - halfWidth, getWindowHeight(),
                             instanceCount == 2 ? GLColor::green : GLColor::blue);
    }

    void drawArrays(GLint first, GLsizei count, GLsizei instanceCount)
    {
        if (instanceCount > 0)
        {
            glDrawArraysInstanced(GL_TRIANGLE_FAN, first, count, instanceCount);
        }
        else
        {
            glDrawArrays(GL_TRIANGLE_FAN, first, count);
        }
    }

    // Draws a fan with the given indices, where kRestart is the primitive restart index of the
    // type, after kPaddingIndices.
    template <typename T>
    void drawElements(GLenum type,
                      const std::vector<GLuint> &fanIndices,
                      Source source,
                      GLsizei instanceCount)
    {
        std::vector<T> indices;
        for (GLuint index : kPaddingIndices)
        {
            indices.push_back(static_cast<T>(index));
        }
        for (GLuint index : fanIndices)
        {
            indices.push_back(index == kRestart ? std::numeric_limits<T>::max()
                                                : static_cast<T>(index));
        }

        const void *indicesPointer = nullptr;
        if (source == Source::Buffer)
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(T), indices.data(),
                         GL_STATIC_DRAW);
            indicesPointer = reinterpret_cast<const void *>(kPaddingIndices.size() * sizeof(T));
        }
        else
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            indicesPointer = ANGLE_UNSAFE_TODO(indices.data() + kPaddingIndices.size());
        }

        const GLsizei count = static_cast<GLsizei>(fanIndices.size());
        if (instanceCount > 0)
        {
            glDrawElementsInstanced(GL_TRIANGLE_FAN, count, type, indicesPointer, instanceCount);
        }
        else
        {
            glDrawElements(GL_TRIANGLE_FAN, count, type, indicesPointer);
        }
    }

    // Draws the fan of kFanIndices with indices of type T, with each source of the vertices and
    // of the indices, and each instance count.
    template <typename T>
    void testDrawElements(GLenum type)
    {
        const std::vector<GLuint> fanIndices(kFanIndices.begin(), kFanIndices.end());
        for (Source vertexSource : sources())
        {
            setPositions(std::vector<GLfloat>(kPositions.begin(), kPositions.end()), vertexSource);
            for (Source indexSource : sources())
            {
                for (GLsizei instanceCount : instanceCounts())
                {
                    SCOPED_TRACE(
                        testing::Message()
                        << "vertices in client memory: " << (vertexSource == Source::ClientMemory)
                        << ", indices in client memory: " << (indexSource == Source::ClientMemory)
                        << ", instance count: " << instanceCount);
                    clear();
                    drawElements<T>(type, fanIndices, indexSource, instanceCount);
                    checkFans(instanceCount);
                }
            }
        }
    }

    // Checks that the fans drew as checkFans() expects when frontFace is GL_CCW, the winding of
    // the fans, and that culling removed them when it is GL_CW.
    void checkFansWithCulling(GLenum frontFace, GLsizei instanceCount)
    {
        if (frontFace == GL_CCW)
        {
            checkFans(instanceCount);
            return;
        }
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth(), getWindowHeight(), GLColor::blue);
    }

    // Draws the fans with back face culling, with glDrawArrays from a first vertex that is not 0
    // and with glDrawElements with each index type, with each front face and instance count.
    void testCullFace()
    {
        const std::vector<GLfloat> arrayPositions = GetPositions({0, 2, 4, 1, 3, 5, 6, 1});
        const std::vector<GLfloat> positions(kPositions.begin(), kPositions.end());
        const std::vector<GLuint> fanIndices(kFanIndices.begin(), kFanIndices.end());
        const bool unsignedIntIndices = unsignedIntIndicesSupported();
        glEnable(GL_CULL_FACE);
        for (GLenum frontFace : {GL_CCW, GL_CW})
        {
            glFrontFace(frontFace);
            for (GLsizei instanceCount : instanceCounts())
            {
                SCOPED_TRACE(testing::Message() << "front face: " << frontFace
                                                << ", instance count: " << instanceCount);
                setPositions(arrayPositions, Source::Buffer);
                clear();
                drawArrays(2, 6, instanceCount);
                checkFansWithCulling(frontFace, instanceCount);

                setPositions(positions, Source::Buffer);
                clear();
                drawElements<GLubyte>(GL_UNSIGNED_BYTE, fanIndices, Source::Buffer, instanceCount);
                checkFansWithCulling(frontFace, instanceCount);
                clear();
                drawElements<GLushort>(GL_UNSIGNED_SHORT, fanIndices, Source::Buffer,
                                       instanceCount);
                checkFansWithCulling(frontFace, instanceCount);
                if (unsignedIntIndices)
                {
                    clear();
                    drawElements<GLuint>(GL_UNSIGNED_INT, fanIndices, Source::Buffer,
                                         instanceCount);
                    checkFansWithCulling(frontFace, instanceCount);
                }

                if (getClientMajorVersion() >= 3)
                {
                    // Two fans with primitive restart, with different first vertices.
                    const std::vector<GLuint> restartFanIndices = {4, 1, 3, 5, kRestart,
                                                                   5, 6, 1, 4};
                    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
                    clear();
                    drawElements<GLubyte>(GL_UNSIGNED_BYTE, restartFanIndices, Source::Buffer,
                                          instanceCount);
                    checkFansWithCulling(frontFace, instanceCount);
                    clear();
                    drawElements<GLushort>(GL_UNSIGNED_SHORT, restartFanIndices, Source::Buffer,
                                           instanceCount);
                    checkFansWithCulling(frontFace, instanceCount);
                    clear();
                    drawElements<GLuint>(GL_UNSIGNED_INT, restartFanIndices, Source::Buffer,
                                         instanceCount);
                    checkFansWithCulling(frontFace, instanceCount);
                    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
                }
            }
        }
    }

    // Draws fans with glDrawArrays with vertex counts whose triangle lists have more indices than
    // 32-bit indices can index, which fail with GL_OUT_OF_MEMORY, and then a fan unless the
    // GL_OUT_OF_MEMORY lost the context.
    void testTooManyIndices()
    {
        // The draws read no vertex attributes.
        glDisableVertexAttribArray(mPositionLocation);
        bool outOfMemoryReported = false;
        // 3 * (1431655768 - 2) is 2^32 + 2, which wraps to 2 in 32 bits.
        for (GLsizei count : {1431655768, std::numeric_limits<GLsizei>::max()})
        {
            for (GLsizei instanceCount : instanceCounts())
            {
                SCOPED_TRACE(testing::Message()
                             << "count: " << count << ", instance count: " << instanceCount);
                drawArrays(0, count, instanceCount);
                const GLenum error = glGetError();
                // Contexts that are lost on backend errors, such as WebGL contexts, report
                // GL_CONTEXT_LOST for the draws after the first GL_OUT_OF_MEMORY.
                if (error == GL_CONTEXT_LOST && outOfMemoryReported)
                {
                    return;
                }
                EXPECT_GLENUM_EQ(GL_OUT_OF_MEMORY, error);
                outOfMemoryReported = outOfMemoryReported || error == GL_OUT_OF_MEMORY;
            }
        }

        glEnableVertexAttribArray(mPositionLocation);
        setPositions(GetPositions({0, 2, 4, 1, 3, 5, 6, 1}), Source::Buffer);
        clear();
        drawArrays(2, 6, 0);
        checkFans(0);
    }

    // Draws two fans that cover the left half with primitive restart, with each source of the
    // vertices and of the indices, and each instance count. Between and around the fans are
    // primitive restart indices and fans with fewer than three vertices, which use the vertices of
    // the right half.
    template <typename T>
    void testPrimitiveRestart(GLenum type)
    {
        // The two fans have different first vertices.
        const std::vector<GLuint> fanIndices = {kRestart, kRestart, 4,        1,        3,       5,
                                                kRestart, 0,        2,        kRestart, 5,       6,
                                                1,        4,        kRestart, 2,        kRestart};
        glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        for (Source vertexSource : sources())
        {
            setPositions(std::vector<GLfloat>(kPositions.begin(), kPositions.end()), vertexSource);
            for (Source indexSource : sources())
            {
                for (GLsizei instanceCount : instanceCounts())
                {
                    SCOPED_TRACE(
                        testing::Message()
                        << "vertices in client memory: " << (vertexSource == Source::ClientMemory)
                        << ", indices in client memory: " << (indexSource == Source::ClientMemory)
                        << ", instance count: " << instanceCount);
                    clear();
                    drawElements<T>(type, fanIndices, indexSource, instanceCount);
                    checkFans(instanceCount);
                }
            }
        }
    }

    // Draws fans with primitive restart that have fewer than three vertices, which draw nothing,
    // and then the fan of kFanIndices.
    template <typename T>
    void testPrimitiveRestartNoTriangles(GLenum type)
    {
        const std::vector<GLuint> noTriangleIndices = {kRestart, 4, 1, kRestart, kRestart, 0,
                                                       kRestart, 2, 2, kRestart, 5};
        const std::vector<GLuint> fanIndices(kFanIndices.begin(), kFanIndices.end());
        glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        setPositions(std::vector<GLfloat>(kPositions.begin(), kPositions.end()), Source::Buffer);
        for (Source indexSource : sources())
        {
            for (GLsizei instanceCount : instanceCounts())
            {
                SCOPED_TRACE(testing::Message() << "indices in client memory: "
                                                << (indexSource == Source::ClientMemory)
                                                << ", instance count: " << instanceCount);
                clear();
                drawElements<T>(type, noTriangleIndices, indexSource, instanceCount);
                ASSERT_GL_NO_ERROR();
                EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth(), getWindowHeight(), GLColor::blue);

                drawElements<T>(type, fanIndices, indexSource, instanceCount);
                checkFans(instanceCount);
            }
        }
    }

    bool mClientArraysEnabled = true;
    GLProgram mProgram;
    GLint mPositionLocation = -1;
    GLBuffer mPositionBuffer;
    GLBuffer mIndexBuffer;
    std::vector<GLfloat> mPositions;
};

// Tests triangle fans with client arrays disabled, as in WebGL contexts.
class WebGLTriangleFanTest : public TriangleFanTest
{
  protected:
    WebGLTriangleFanTest()
    {
        setWebGLCompatibilityEnabled(true);
        setClientArraysEnabled(false);
        mClientArraysEnabled = false;
    }
};

// Draws a fan with glDrawArrays from a first vertex that is not 0, with each source of the
// vertices and each instance count.
TEST_P(TriangleFanTest, DrawArrays)
{
    // The fan starts at vertex 2. A fan that started at vertex 0 would use the vertices 0 and 1.
    const std::vector<GLfloat> positions = GetPositions({0, 2, 4, 1, 3, 5, 6, 1});
    for (Source vertexSource : sources())
    {
        setPositions(positions, vertexSource);
        for (GLsizei instanceCount : instanceCounts())
        {
            SCOPED_TRACE(testing::Message()
                         << "vertices in client memory: " << (vertexSource == Source::ClientMemory)
                         << ", instance count: " << instanceCount);
            clear();
            drawArrays(2, 6, instanceCount);
            checkFans(instanceCount);
        }
    }
}

// Draws a fan with glDrawArrays from a first vertex that does not fit in 16 bits.
TEST_P(TriangleFanTest, DrawArraysLargeFirst)
{
    constexpr GLint kFirst = 70000;
    std::vector<GLuint> vertices(kFirst, 0);
    vertices.insert(vertices.end(), kFanIndices.begin(), kFanIndices.end());
    const std::vector<GLfloat> positions = GetPositions(vertices);
    for (Source vertexSource : sources())
    {
        setPositions(positions, vertexSource);
        for (GLsizei instanceCount : instanceCounts())
        {
            SCOPED_TRACE(testing::Message()
                         << "vertices in client memory: " << (vertexSource == Source::ClientMemory)
                         << ", instance count: " << instanceCount);
            clear();
            drawArrays(kFirst, static_cast<GLsizei>(kFanIndices.size()), instanceCount);
            checkFans(instanceCount);
        }
    }
}

// Draws a fan with glDrawArrays with more vertices than 16-bit indices can index. The vertices
// after the center go around the left half.
TEST_P(TriangleFanTest, DrawArraysManyVertices)
{
    constexpr size_t kVerticesPerSide   = 20000;
    const std::array<GLuint, 5> corners = {1, 3, 5, 6, 1};
    std::vector<GLfloat> positions      = GetPositions({0, 4});
    for (size_t side = 0; side + 1 < corners.size(); ++side)
    {
        const std::vector<GLfloat> from = GetPositions({corners[side]});
        const std::vector<GLfloat> to   = GetPositions({corners[side + 1]});
        for (size_t vertex = 0; vertex < kVerticesPerSide; ++vertex)
        {
            const GLfloat t = static_cast<GLfloat>(vertex) / kVerticesPerSide;
            positions.push_back(from[0] + t * (to[0] - from[0]));
            positions.push_back(from[1] + t * (to[1] - from[1]));
        }
    }
    const std::vector<GLfloat> last = GetPositions({corners.back()});
    positions.insert(positions.end(), last.begin(), last.end());
    const GLsizei count = static_cast<GLsizei>(positions.size() / 2 - 1);
    ASSERT_GT(count, 0xFFFF);

    for (Source vertexSource : sources())
    {
        setPositions(positions, vertexSource);
        for (GLsizei instanceCount : instanceCounts())
        {
            SCOPED_TRACE(testing::Message()
                         << "vertices in client memory: " << (vertexSource == Source::ClientMemory)
                         << ", instance count: " << instanceCount);
            clear();
            drawArrays(1, count, instanceCount);
            checkFans(instanceCount);
        }
    }
}

// Draws a fan with 8-bit indices.
TEST_P(TriangleFanTest, DrawElementsUnsignedByte)
{
    testDrawElements<GLubyte>(GL_UNSIGNED_BYTE);
}

// Draws a fan with 16-bit indices.
TEST_P(TriangleFanTest, DrawElementsUnsignedShort)
{
    testDrawElements<GLushort>(GL_UNSIGNED_SHORT);
}

// Draws a fan with 32-bit indices.
TEST_P(TriangleFanTest, DrawElementsUnsignedInt)
{
    ANGLE_SKIP_TEST_IF(!unsignedIntIndicesSupported());
    testDrawElements<GLuint>(GL_UNSIGNED_INT);
}

// Draws a fan with the 8-bit index 0xFF, which is a vertex when primitive restart is disabled.
TEST_P(TriangleFanTest, DrawElementsUnsignedByteIndex255)
{
    // Vertex 255 is the center of the left half.
    std::vector<GLuint> vertices(256, 0);
    for (size_t index = 0; index < kPositions.size() / 2; ++index)
    {
        vertices[index] = static_cast<GLuint>(index);
    }
    vertices[255] = kFanIndices[0];
    std::vector<GLuint> fanIndices(kFanIndices.begin(), kFanIndices.end());
    fanIndices[0] = 255;

    for (Source vertexSource : sources())
    {
        setPositions(GetPositions(vertices), vertexSource);
        for (Source indexSource : sources())
        {
            SCOPED_TRACE(testing::Message()
                         << "vertices in client memory: " << (vertexSource == Source::ClientMemory)
                         << ", indices in client memory: "
                         << (indexSource == Source::ClientMemory));
            clear();
            drawElements<GLubyte>(GL_UNSIGNED_BYTE, fanIndices, indexSource, 0);
            checkFans(0);
        }
    }
}

// Draws a fan with indices from a buffer, then triangles with other indices from the same buffer
// that cover the right half.
TEST_P(TriangleFanTest, DrawElementsThenTriangles)
{
    setPositions(std::vector<GLfloat>(kPositions.begin(), kPositions.end()), Source::Buffer);
    for (GLenum type : {GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT})
    {
        SCOPED_TRACE(type);
        // The fan, then the triangles of the right half.
        const std::vector<GLuint> indices = {4, 1, 3, 5, 6, 1, 3, 0, 5, 5, 0, 2};
        const size_t indexSize = type == GL_UNSIGNED_BYTE ? sizeof(GLubyte) : sizeof(GLushort);
        std::vector<GLubyte> unsignedByteIndices(indices.begin(), indices.end());
        std::vector<GLushort> unsignedShortIndices(indices.begin(), indices.end());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);
        if (type == GL_UNSIGNED_BYTE)
        {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, unsignedByteIndices.size(),
                         unsignedByteIndices.data(), GL_STATIC_DRAW);
        }
        else
        {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, unsignedShortIndices.size() * sizeof(GLushort),
                         unsignedShortIndices.data(), GL_STATIC_DRAW);
        }

        clear();
        glDrawElements(GL_TRIANGLE_FAN, 6, type, nullptr);
        glDrawElements(GL_TRIANGLES, 6, type, reinterpret_cast<const void *>(6 * indexSize));
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth(), getWindowHeight(), GLColor::green);
    }
}

// Draws fans with back face culling. The fans are counterclockwise.
TEST_P(TriangleFanTest, CullFace)
{
    testCullFace();
}

// Draws fans whose triangle lists have more indices than 32-bit indices can index.
TEST_P(TriangleFanTest, TooManyIndices)
{
    // Only the backends that emulate triangle fans with index buffers need the indices.
    ANGLE_SKIP_TEST_IF(!IsWebGPU() && !IsMetal());
    testTooManyIndices();
}

// Draws fans whose vertices check that gl_VertexID is their index, with glDrawArrays from a first
// vertex that is not 0 and with glDrawElements, and with each instance count.
TEST_P(TriangleFanTest, VertexID)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    constexpr char kVS[] = R"(#version 300 es
in vec2 a_position;
in float a_vertexId;
out vec4 v_color;
void main()
{
    gl_Position = vec4(a_position.x + float(gl_InstanceID), a_position.y, 0.0, 1.0);
    v_color = int(a_vertexId) == gl_VertexID ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
})";
    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
in vec4 v_color;
out vec4 color;
void main()
{
    color = v_color;
})";
    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);
    mPositionLocation            = glGetAttribLocation(program, "a_position");
    const GLint vertexIdLocation = glGetAttribLocation(program, "a_vertexId");
    ASSERT_NE(-1, mPositionLocation);
    ASSERT_NE(-1, vertexIdLocation);
    glEnableVertexAttribArray(mPositionLocation);

    std::vector<GLfloat> vertexIds(8);
    std::iota(vertexIds.begin(), vertexIds.end(), 0.0f);
    GLBuffer vertexIdBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexIdBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertexIds.size() * sizeof(GLfloat), vertexIds.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(vertexIdLocation, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(vertexIdLocation);

    const std::vector<GLuint> fanIndices(kFanIndices.begin(), kFanIndices.end());
    for (GLsizei instanceCount : instanceCounts())
    {
        SCOPED_TRACE(instanceCount);
        setPositions(GetPositions({0, 2, 4, 1, 3, 5, 6, 1}), Source::Buffer);
        clear();
        drawArrays(2, 6, instanceCount);
        checkFans(instanceCount);

        setPositions(std::vector<GLfloat>(kPositions.begin(), kPositions.end()), Source::Buffer);
        clear();
        drawElements<GLushort>(GL_UNSIGNED_SHORT, fanIndices, Source::Buffer, instanceCount);
        checkFans(instanceCount);
    }
}

// Draws fans with primitive restart with 8-bit indices.
TEST_P(TriangleFanTest, PrimitiveRestartUnsignedByte)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    testPrimitiveRestart<GLubyte>(GL_UNSIGNED_BYTE);
}

// Draws fans with primitive restart with 16-bit indices.
TEST_P(TriangleFanTest, PrimitiveRestartUnsignedShort)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    testPrimitiveRestart<GLushort>(GL_UNSIGNED_SHORT);
}

// Draws fans with primitive restart with 32-bit indices.
TEST_P(TriangleFanTest, PrimitiveRestartUnsignedInt)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    testPrimitiveRestart<GLuint>(GL_UNSIGNED_INT);
}

// Draws fans with primitive restart and no triangles with 8-bit indices, then a fan.
TEST_P(TriangleFanTest, PrimitiveRestartNoTrianglesUnsignedByte)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    testPrimitiveRestartNoTriangles<GLubyte>(GL_UNSIGNED_BYTE);
}

// Draws fans with primitive restart and no triangles with 16-bit indices, then a fan.
TEST_P(TriangleFanTest, PrimitiveRestartNoTrianglesUnsignedShort)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    testPrimitiveRestartNoTriangles<GLushort>(GL_UNSIGNED_SHORT);
}

// Draws fans with primitive restart and no triangles with 32-bit indices, then a fan.
TEST_P(TriangleFanTest, PrimitiveRestartNoTrianglesUnsignedInt)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    testPrimitiveRestartNoTriangles<GLuint>(GL_UNSIGNED_INT);
}

// Draws a fan with glDrawArrays from a first vertex that is not 0, with client arrays disabled.
TEST_P(WebGLTriangleFanTest, DrawArrays)
{
    setPositions(GetPositions({0, 2, 4, 1, 3, 5, 6, 1}), Source::Buffer);
    for (GLsizei instanceCount : instanceCounts())
    {
        SCOPED_TRACE(instanceCount);
        clear();
        drawArrays(2, 6, instanceCount);
        checkFans(instanceCount);
    }
}

// Draws a fan with 8-bit indices, with client arrays disabled.
TEST_P(WebGLTriangleFanTest, DrawElementsUnsignedByte)
{
    testDrawElements<GLubyte>(GL_UNSIGNED_BYTE);
}

// Draws a fan with 16-bit indices, with client arrays disabled.
TEST_P(WebGLTriangleFanTest, DrawElementsUnsignedShort)
{
    testDrawElements<GLushort>(GL_UNSIGNED_SHORT);
}

// Draws a fan with 32-bit indices, with client arrays disabled.
TEST_P(WebGLTriangleFanTest, DrawElementsUnsignedInt)
{
    ANGLE_SKIP_TEST_IF(!unsignedIntIndicesSupported());
    testDrawElements<GLuint>(GL_UNSIGNED_INT);
}

// Draws fans with back face culling, with client arrays disabled.
TEST_P(WebGLTriangleFanTest, CullFace)
{
    testCullFace();
}

// Draws fans whose triangle lists have more indices than 32-bit indices can index, with client
// arrays disabled.
TEST_P(WebGLTriangleFanTest, TooManyIndices)
{
    // Only the backends that emulate triangle fans with index buffers need the indices.
    ANGLE_SKIP_TEST_IF(!IsWebGPU() && !IsMetal());
    testTooManyIndices();
}

// Draws fans with primitive restart with 8-bit indices, with client arrays disabled.
TEST_P(WebGLTriangleFanTest, PrimitiveRestartUnsignedByte)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    testPrimitiveRestart<GLubyte>(GL_UNSIGNED_BYTE);
}

// Draws fans with primitive restart with 16-bit indices, with client arrays disabled.
TEST_P(WebGLTriangleFanTest, PrimitiveRestartUnsignedShort)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    testPrimitiveRestart<GLushort>(GL_UNSIGNED_SHORT);
}

// Draws fans with primitive restart with 32-bit indices, with client arrays disabled.
TEST_P(WebGLTriangleFanTest, PrimitiveRestartUnsignedInt)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    testPrimitiveRestart<GLuint>(GL_UNSIGNED_INT);
}

// Draws fans with primitive restart and no triangles with 8-bit indices, then a fan, with client
// arrays disabled.
TEST_P(WebGLTriangleFanTest, PrimitiveRestartNoTrianglesUnsignedByte)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    testPrimitiveRestartNoTriangles<GLubyte>(GL_UNSIGNED_BYTE);
}

// Draws fans with primitive restart and no triangles with 16-bit indices, then a fan, with client
// arrays disabled.
TEST_P(WebGLTriangleFanTest, PrimitiveRestartNoTrianglesUnsignedShort)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    testPrimitiveRestartNoTriangles<GLushort>(GL_UNSIGNED_SHORT);
}

// Draws fans with primitive restart and no triangles with 32-bit indices, then a fan, with client
// arrays disabled.
TEST_P(WebGLTriangleFanTest, PrimitiveRestartNoTrianglesUnsignedInt)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    testPrimitiveRestartNoTriangles<GLuint>(GL_UNSIGNED_INT);
}

// Simple repeated draw and swap test.
TEST_P(SimpleOperationTest, DrawQuadAndSwap)
{
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);

    for (int i = 0; i < 8; ++i)
    {
        drawQuad(program, "position", 0.5f, 1.0f, true);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
        swapBuffers();
    }

    ASSERT_GL_NO_ERROR();
}

// Simple indexed quad test.
TEST_P(SimpleOperationTest, DrawIndexedQuad)
{
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);

    drawIndexedQuad(program, "position", 0.5f, 1.0f, true);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Simple repeated indexed draw and swap test.
TEST_P(SimpleOperationTest, DrawIndexedQuadAndSwap)
{
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);

    // 32 iterations is an arbitrary number. The more iterations, the more flaky syncronization
    // issues will reproduce consistently.
    for (int i = 0; i < 32; ++i)
    {
        drawIndexedQuad(program, "position", 0.5f, 1.0f, true);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
        swapBuffers();
    }

    ASSERT_GL_NO_ERROR();
}

// Draw with a fragment uniform.
TEST_P(SimpleOperationTest, DrawQuadWithFragmentUniform)
{
    constexpr char kFS[] =
        "uniform mediump vec4 color;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = color;\n"
        "}";
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kFS);

    GLint location = glGetUniformLocation(program, "color");
    ASSERT_NE(-1, location);

    glUseProgram(program);
    glUniform4f(location, 0.0f, 1.0f, 0.0f, 1.0f);

    drawQuad(program, "position", 0.5f, 1.0f, true);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Draw with a vertex uniform.
TEST_P(SimpleOperationTest, DrawQuadWithVertexUniform)
{
    constexpr char kVS[] =
        "attribute vec3 position;\n"
        "uniform vec4 color;\n"
        "varying vec4 vcolor;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 1);\n"
        "    vcolor = color;\n"
        "}";
    constexpr char kFS[] =
        "varying mediump vec4 vcolor;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vcolor;\n"
        "}";
    ANGLE_GL_PROGRAM(program, kVS, kFS);

    const GLint location = glGetUniformLocation(program, "color");
    ASSERT_NE(-1, location);

    glUseProgram(program);
    glUniform4f(location, 0.0f, 1.0f, 0.0f, 1.0f);

    drawQuad(program, "position", 0.5f, 1.0f, true);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Draw with two uniforms.
TEST_P(SimpleOperationTest, DrawQuadWithTwoUniforms)
{
    constexpr char kVS[] =
        "attribute vec3 position;\n"
        "uniform vec4 color1;\n"
        "varying vec4 vcolor1;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 1);\n"
        "    vcolor1 = color1;\n"
        "}";
    constexpr char kFS[] =
        "uniform mediump vec4 color2;\n"
        "varying mediump vec4 vcolor1;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vcolor1 + color2;\n"
        "}";
    ANGLE_GL_PROGRAM(program, kVS, kFS);

    const GLint location1 = glGetUniformLocation(program, "color1");
    ASSERT_NE(-1, location1);

    const GLint location2 = glGetUniformLocation(program, "color2");
    ASSERT_NE(-1, location2);

    glUseProgram(program);
    glUniform4f(location1, 0.0f, 1.0f, 0.0f, 1.0f);
    glUniform4f(location2, 1.0f, 0.0f, 0.0f, 1.0f);

    drawQuad(program, "position", 0.5f, 1.0f, true);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::yellow);
}

// Tests a shader program with more than one vertex attribute, with vertex buffers.
TEST_P(SimpleOperationTest, ThreeVertexAttributes)
{
    constexpr char kVS[] = R"(attribute vec2 position;
attribute vec4 color1;
attribute vec4 color2;
varying vec4 color;
void main()
{
    gl_Position = vec4(position, 0, 1);
    color = color1 + color2;
})";

    constexpr char kFS[] = R"(precision mediump float;
varying vec4 color;
void main()
{
    gl_FragColor = color;
}
)";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    glUseProgram(program);

    const GLint color1Loc = glGetAttribLocation(program, "color1");
    const GLint color2Loc = glGetAttribLocation(program, "color2");
    ASSERT_NE(-1, color1Loc);
    ASSERT_NE(-1, color2Loc);

    const auto &indices = GetQuadIndices();

    // Make colored corners with red == x or 1 -x , and green = y or 1 - y.

    std::array<GLColor, 4> baseColors1 = {
        {GLColor::black, GLColor::red, GLColor::green, GLColor::yellow}};
    std::array<GLColor, 4> baseColors2 = {
        {GLColor::yellow, GLColor::green, GLColor::red, GLColor::black}};

    std::vector<GLColor> colors1;
    std::vector<GLColor> colors2;

    for (GLushort index : indices)
    {
        colors1.push_back(baseColors1[index]);
        colors2.push_back(baseColors2[index]);
    }

    GLBuffer color1Buffer;
    glBindBuffer(GL_ARRAY_BUFFER, color1Buffer);
    glBufferData(GL_ARRAY_BUFFER, colors1.size() * sizeof(GLColor), colors1.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(color1Loc, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, nullptr);
    glEnableVertexAttribArray(color1Loc);

    GLBuffer color2Buffer;
    glBindBuffer(GL_ARRAY_BUFFER, color2Buffer);
    glBufferData(GL_ARRAY_BUFFER, colors2.size() * sizeof(GLColor), colors2.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(color2Loc, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, nullptr);
    glEnableVertexAttribArray(color2Loc);

    // Draw a non-indexed quad with all vertex buffers. Should draw yellow to the entire window.
    drawQuad(program, "position", 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth(), getWindowHeight(), GLColor::yellow);
}

// Creates a 2D texture, no other operations.
TEST_P(SimpleOperationTest, CreateTexture2DNoData)
{
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();
}

// Creates a 2D texture, no other operations.
TEST_P(SimpleOperationTest, CreateTexture2DWithData)
{
    std::vector<GLColor> colors(16 * 16, GLColor::red);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors.data());
    ASSERT_GL_NO_ERROR();
}

// Creates a cube texture, no other operations.
TEST_P(SimpleOperationTest, CreateTextureCubeNoData)
{
    GLTexture texture;
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    for (GLenum cubeFace : kCubeFaces)
    {
        glTexImage2D(cubeFace, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
    ASSERT_GL_NO_ERROR();
}

// Creates a cube texture, no other operations.
TEST_P(SimpleOperationTest, CreateTextureCubeWithData)
{
    std::vector<GLColor> colors(16 * 16, GLColor::red);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    for (GLenum cubeFace : kCubeFaces)
    {
        glTexImage2D(cubeFace, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors.data());
    }
    ASSERT_GL_NO_ERROR();
}

// Creates a program with a texture.
TEST_P(SimpleOperationTest, LinkProgramWithTexture)
{
    ASSERT_NE(0u, get2DTexturedQuadProgram());
    ASSERT_GL_NO_ERROR();
}

// Creates a program with a 2D texture and renders with it.
TEST_P(SimpleOperationTest, DrawWith2DTexture)
{
    std::array<GLColor, 4> colors = {
        {GLColor::red, GLColor::green, GLColor::blue, GLColor::yellow}};

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    draw2DTexturedQuad(0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    int w = getWindowWidth() - 2;
    int h = getWindowHeight() - 2;

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(w, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, h, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(w, h, GLColor::yellow);
}

template <typename T>
void SimpleOperationTest::testDrawElementsLineLoopUsingClientSideMemory(GLenum indexType,
                                                                        int windowWidth,
                                                                        int windowHeight)
{
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);
    glUseProgram(program);

    // We expect to draw a square with these 4 vertices with a drawArray call.
    std::vector<Vector3> vertices;
    CreatePixelCenterWindowCoords({{32, 96}, {32, 32}, {96, 32}, {96, 96}}, windowWidth,
                                  windowHeight, &vertices);

    // If we use these indices to draw however, we should be drawing an hourglass.
    std::vector<T> indices{3, 2, 1, 0};

    GLint positionLocation = glGetAttribLocation(program, "position");
    ASSERT_NE(-1, positionLocation);

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(positionLocation);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawElements(GL_LINE_LOOP, 4, indexType, indices.data());
    glDisableVertexAttribArray(positionLocation);

    ASSERT_GL_NO_ERROR();

    int quarterWidth  = windowWidth / 4;
    int quarterHeight = windowHeight / 4;

    // Bottom left
    EXPECT_PIXEL_COLOR_EQ(quarterWidth, quarterHeight, GLColor::green);

    // Top left
    EXPECT_PIXEL_COLOR_EQ(quarterWidth, (quarterHeight * 3), GLColor::green);

    // Top right
    EXPECT_PIXEL_COLOR_EQ((quarterWidth * 3), (quarterHeight * 3) - 1, GLColor::green);

    // Verify line is closed between the 2 last vertices
    EXPECT_PIXEL_COLOR_EQ((quarterWidth * 2), quarterHeight, GLColor::green);
}

// Draw a line loop using a drawElement call and client side memory.
TEST_P(SimpleOperationTest, DrawElementsLineLoopUsingUShortClientSideMemory)
{
    testDrawElementsLineLoopUsingClientSideMemory<GLushort>(GL_UNSIGNED_SHORT, getWindowWidth(),
                                                            getWindowHeight());
}

// Draw a line loop using a drawElement call and client side memory.
TEST_P(SimpleOperationTest, DrawElementsLineLoopUsingUByteClientSideMemory)
{
    testDrawElementsLineLoopUsingClientSideMemory<GLubyte>(GL_UNSIGNED_BYTE, getWindowWidth(),
                                                           getWindowHeight());
}

// Creates a program with a cube texture and renders with it.
TEST_P(SimpleOperationTest, DrawWithCubeTexture)
{
    std::array<Vector2, 6 * 4> positions = {{
        {0, 1}, {1, 1}, {1, 2}, {0, 2} /* first face */,
        {1, 0}, {2, 0}, {2, 1}, {1, 1} /* second face */,
        {1, 1}, {2, 1}, {2, 2}, {1, 2} /* third face */,
        {1, 2}, {2, 2}, {2, 3}, {1, 3} /* fourth face */,
        {2, 1}, {3, 1}, {3, 2}, {2, 2} /* fifth face */,
        {3, 1}, {4, 1}, {4, 2}, {3, 2} /* sixth face */,
    }};

    const float w4 = 1.0f / 4.0f;
    const float h3 = 1.0f / 3.0f;

    // This draws a "T" shape based on the four faces of the cube. The window is divided into four
    // tiles horizontally and three tiles vertically (hence the w4 and h3 variable naming).
    for (Vector2 &pos : positions)
    {
        ANGLE_UNSAFE_TODO({
            pos.data()[0] = pos.data()[0] * w4 * 2.0f - 1.0f;
            pos.data()[1] = pos.data()[1] * h3 * 2.0f - 1.0f;
        })
    }

    const Vector3 posX(1, 0, 0);
    const Vector3 negX(-1, 0, 0);
    const Vector3 posY(0, 1, 0);
    const Vector3 negY(0, -1, 0);
    const Vector3 posZ(0, 0, 1);
    const Vector3 negZ(0, 0, -1);

    std::array<Vector3, 6 * 4> coords = {{
        posX, posX, posX, posX /* first face */, negX, negX, negX, negX /* second face */,
        posY, posY, posY, posY /* third face */, negY, negY, negY, negY /* fourth face */,
        posZ, posZ, posZ, posZ /* fifth face */, negZ, negZ, negZ, negZ /* sixth face */,
    }};

    const std::array<std::array<GLColor, 4>, 6> colors = {{
        {GLColor::red, GLColor::red, GLColor::red, GLColor::red},
        {GLColor::green, GLColor::green, GLColor::green, GLColor::green},
        {GLColor::blue, GLColor::blue, GLColor::blue, GLColor::blue},
        {GLColor::yellow, GLColor::yellow, GLColor::yellow, GLColor::yellow},
        {GLColor::cyan, GLColor::cyan, GLColor::cyan, GLColor::cyan},
        {GLColor::magenta, GLColor::magenta, GLColor::magenta, GLColor::magenta},
    }};

    GLTexture texture;
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

    for (size_t faceIndex = 0; faceIndex < kCubeFaces.size(); ++faceIndex)
    {
        glTexImage2D(kCubeFaces[faceIndex], 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     colors[faceIndex].data());
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    constexpr char kVertexShader[] = R"(attribute vec2 pos;
attribute vec3 coord;
varying vec3 texCoord;
void main()
{
    gl_Position = vec4(pos, 0, 1);
    texCoord = coord;
})";

    constexpr char kFragmentShader[] = R"(precision mediump float;
varying vec3 texCoord;
uniform samplerCube tex;
void main()
{
    gl_FragColor = textureCube(tex, texCoord);
})";

    ANGLE_GL_PROGRAM(program, kVertexShader, kFragmentShader);
    GLint samplerLoc = glGetUniformLocation(program, "tex");
    ASSERT_EQ(samplerLoc, 0);

    glUseProgram(program);

    GLint posLoc = glGetAttribLocation(program, "pos");
    ASSERT_NE(-1, posLoc);

    GLint coordLoc = glGetAttribLocation(program, "coord");
    ASSERT_NE(-1, coordLoc);

    GLBuffer posBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, posBuffer);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(Vector2), positions.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(posLoc);

    GLBuffer coordBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, coordBuffer);
    glBufferData(GL_ARRAY_BUFFER, coords.size() * sizeof(Vector3), coords.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(coordLoc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(coordLoc);

    auto quadIndices = GetQuadIndices();
    std::array<GLushort, 6 * 6> kElementsData;
    for (GLushort quadIndex = 0; quadIndex < 6; ++quadIndex)
    {
        for (GLushort elementIndex = 0; elementIndex < 6; ++elementIndex)
        {
            kElementsData[quadIndex * 6 + elementIndex] = quadIndices[elementIndex] + 4 * quadIndex;
        }
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLBuffer elementBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, kElementsData.size() * sizeof(GLushort),
                 kElementsData.data(), GL_STATIC_DRAW);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(kElementsData.size()), GL_UNSIGNED_SHORT,
                   nullptr);
    ASSERT_GL_NO_ERROR();

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex)
    {
        int index      = faceIndex * 4;
        Vector2 center = (positions[index] + positions[index + 1] + positions[index + 2] +
                          positions[index + 3]) /
                         4.0f;
        center *= 0.5f;
        center += Vector2(0.5f);
        center *= Vector2(getWindowWidth(), getWindowHeight());
        EXPECT_PIXEL_COLOR_EQ(static_cast<GLint>(center.x()), static_cast<GLint>(center.y()),
                              colors[faceIndex][0]);
    }
}

// Tests rendering to a user framebuffer.
TEST_P(SimpleOperationTest, RenderToTexture)
{
    constexpr int kSize = 16;

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    ASSERT_GL_NO_ERROR();
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    glViewport(0, 0, kSize, kSize);

    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);
    drawQuad(program, "position", 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Create a simple basic Renderbuffer.
TEST_P(SimpleOperationTest, CreateRenderbuffer)
{
    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);
    ASSERT_GL_NO_ERROR();
}

// Render to a simple color Renderbuffer.
TEST_P(SimpleOperationTest, RenderbufferAttachment)
{
    constexpr int kSize = 16;

    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kSize, kSize);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);
    ASSERT_GL_NO_ERROR();
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    glViewport(0, 0, kSize, kSize);

    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);
    drawQuad(program, "position", 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests that using desktop GL_QUADS/GL_POLYGONS enums generate the correct error.
TEST_P(SimpleOperationTest, PrimitiveModeNegativeTest)
{
    // Draw a correct quad.
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);
    glUseProgram(program);

    GLint positionLocation = glGetAttribLocation(program, "position");
    ASSERT_NE(-1, positionLocation);

    setupQuadVertexBuffer(0.5f, 1.0f);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(positionLocation);

    // Tests that TRIANGLES works.
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Tests that specific invalid enums don't work.
    glDrawArrays(static_cast<GLenum>(7), 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glDrawArrays(static_cast<GLenum>(8), 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glDrawArrays(static_cast<GLenum>(9), 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Tests that using GL_LINES_ADJACENCY should not crash the app even if the backend doesn't support
// LinesAdjacent mode.
// This is to verify that the crash in crbug.com/1457840 won't happen.
TEST_P(SimpleOperationTest, PrimitiveModeLinesAdjacentNegativeTest)
{
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);
    glUseProgram(program);

    GLint positionLocation = glGetAttribLocation(program, "position");
    ASSERT_NE(-1, positionLocation);

    setupQuadVertexBuffer(0.5f, 1.0f);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(positionLocation);

    {
        // Tests that TRIANGLES works.
        glDrawArrays(GL_TRIANGLES, 0, 6);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    }

    {
        // Drawing with GL_LINES_ADJACENCY won't crash even if the backend doesn't support it.
        glDrawArrays(GL_LINES_ADJACENCY, 0, 6);

        if (IsGLExtensionEnabled("GL_ANGLE_instanced_arrays"))
        {
            glDrawArraysInstancedANGLE(GL_LINES_ADJACENCY, 0, 6, 2);
        }
    }

    // Indexed draws with GL_LINES_ADJACENCY
    setupIndexedQuadVertexBuffer(0.5f, 1.0f);
    setupIndexedQuadIndexBuffer();

    // Clear GL error state, since ahove calls may have set GL_INVALID_ENUM.
    glGetError();

    {
        // Tests that TRIANGLES works.
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    }

    {
        // Indexed drawing with GL_LINES_ADJACENCY won't crash even if the backend doesn't support
        // it.
        glDrawElements(GL_LINES_ADJACENCY, 6, GL_UNSIGNED_SHORT, nullptr);
        if (IsGLExtensionEnabled("GL_ANGLE_instanced_arrays"))
        {
            glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr, 1);
        }
    }
}

// Tests all primitive modes, including some invalid ones, with a vertex shader that receives
// no data for its vertex attributes (constant values). This is a port of the test case from
// crbug.com/1457840, exercising slightly different code paths.
TEST_P(SimpleOperationTest, DrawsWithNoAttributeData)
{
    constexpr char kTestVertexShader[] =
        R"(attribute vec4 vPosition;
attribute vec2 texCoord0;
varying vec2 texCoord;
void main() {
    gl_Position = vPosition;
    texCoord = texCoord0;
})";
    constexpr char kTestFragmentShader[] =
        R"(precision mediump float;
uniform sampler2D tex;
uniform float divisor;
varying vec2 texCoord;
void main() {
    gl_FragData[0] = texture2DProj(tex, vec3(texCoord, divisor));
})";

    constexpr std::array<GLenum, 12> kPrimitiveModes = {
        {GL_POINTS, GL_LINES, GL_LINE_LOOP, GL_LINE_STRIP, GL_TRIANGLES, GL_TRIANGLE_STRIP,
         GL_TRIANGLE_FAN,

         // Illegal or possibly-illegal modes
         GL_QUERY_RESULT, GL_LINES_ADJACENCY, GL_LINE_STRIP_ADJACENCY, GL_TRIANGLES_ADJACENCY,
         GL_TRIANGLE_STRIP_ADJACENCY}};

    ANGLE_GL_PROGRAM(program, kTestVertexShader, kTestFragmentShader);
    glUseProgram(program);

    for (GLenum mode : kPrimitiveModes)
    {
        glDrawArrays(mode, 0, 0x8866);
    }

    // If the test reaches this point then it hasn't crashed.
}

// Verify we don't crash when attempting to draw using GL_TRIANGLES without a program bound.
TEST_P(SimpleOperationTest31, DrawTrianglesWithoutProgramBound)
{
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// Verify we don't crash when attempting to draw using GL_LINE_STRIP_ADJACENCY without a program
// bound.
TEST_P(SimpleOperationTest31, DrawLineStripAdjacencyWithoutProgramBound)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, 10);
}

// Verify instanceCount == 0 is no-op
TEST_P(SimpleOperationTest, DrawArraysZeroInstanceCountIsNoOp)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_instanced_arrays"));

    // Draw a correct green quad.
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);
    glUseProgram(program);

    GLint positionLocation = glGetAttribLocation(program, "position");
    ASSERT_NE(-1, positionLocation);

    setupQuadVertexBuffer(0.5f, 1.0f);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(positionLocation);

    // If nothing is drawn it should be red
    glClearColor(1.0, 0.0, 0.0, 1.0);

    {
        // Non-instanced draw should draw
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    }
    {
        // instanceCount == 0 should be no-op
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArraysInstancedANGLE(GL_TRIANGLES, 0, 6, 0);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    }
    {
        // instanceCount > 0 should draw
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArraysInstancedANGLE(GL_TRIANGLES, 0, 6, 1);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    }
}

// Verify instanceCount == 0 is no-op
TEST_P(SimpleOperationTest, DrawElementsZeroInstanceCountIsNoOp)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_instanced_arrays"));

    // Draw a correct green quad.
    ANGLE_GL_PROGRAM(program, kBasicVertexShader, kGreenFragmentShader);
    glUseProgram(program);

    GLint positionLocation = glGetAttribLocation(program, "position");
    ASSERT_NE(-1, positionLocation);

    setupIndexedQuadVertexBuffer(0.5f, 1.0f);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(positionLocation);

    setupIndexedQuadIndexBuffer();

    // If nothing is drawn it should be red
    glClearColor(1.0, 0.0, 0.0, 1.0);

    {
        // Non-instanced draw should draw
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    }
    {
        // instanceCount == 0 should be no-op
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr, 0);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    }
    {
        // instanceCount > 0 should draw
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr, 1);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    }
}

// Test that sample coverage does not affect single sample rendering
TEST_P(SimpleOperationTest, DrawSingleSampleWithCoverage)
{
    GLint sampleBuffers = -1;
    glGetIntegerv(GL_SAMPLE_BUFFERS, &sampleBuffers);
    ASSERT_EQ(sampleBuffers, 0);

    GLint samples = -1;
    glGetIntegerv(GL_SAMPLES, &samples);
    ASSERT_EQ(samples, 0);

    glClearColor(1.0, 0.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_SAMPLE_COVERAGE);
    glSampleCoverage(0.0f, false);

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.0);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that sample coverage affects multi sample rendering with only one sample
TEST_P(SimpleOperationTest, DrawSingleMultiSampleWithCoverage)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLRenderbuffer rbo;
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 1, GL_RGBA8, 1, 1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    GLint samples = -1;
    glGetIntegerv(GL_SAMPLES, &samples);
    ASSERT_GT(samples, 0);
    ANGLE_SKIP_TEST_IF(samples != 1);

    glClearColor(0.0, 0.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_SAMPLE_COVERAGE);
    glSampleCoverage(0.0f, false);

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.0);
    ASSERT_GL_NO_ERROR();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
}

// Test that alpha-to-coverage does not affect single-sampled rendering
TEST_P(SimpleOperationTest, DrawSingleSampleWithAlphaToCoverage)
{
    GLint sampleBuffers = -1;
    glGetIntegerv(GL_SAMPLE_BUFFERS, &sampleBuffers);
    ASSERT_EQ(sampleBuffers, 0);

    GLint samples = -1;
    glGetIntegerv(GL_SAMPLES, &samples);
    ASSERT_EQ(samples, 0);

    glClearColor(1.0, 0.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
    glUseProgram(program);
    glUniform4f(glGetUniformLocation(program, essl1_shaders::ColorUniform()), 0, 1, 0, 0);
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.0);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(0, 255, 0, 0));
}

// Test that alpha-to-coverage affects multisampled rendering with only one sample
TEST_P(SimpleOperationTest, DrawSingleMultiSampleWithAlphaToCoverage)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLRenderbuffer rbo;
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 1, GL_RGBA8, 1, 1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    GLint samples = -1;
    glGetIntegerv(GL_SAMPLES, &samples);
    ASSERT_GT(samples, 0);
    ANGLE_SKIP_TEST_IF(samples != 1);

    glClearColor(0.0, 0.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
    glUseProgram(program);
    glUniform4f(glGetUniformLocation(program, essl1_shaders::ColorUniform()), 0, 1, 0, 0);
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.0);
    ASSERT_GL_NO_ERROR();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
}

// Test glGetRenderbufferParameteriv returned value for different render buffer
TEST_P(SimpleOperationTest, GetRenderbufferParameter)
{
    GLint value = 0;
    // call glGetRenderbufferParameteriv with renderbuffer bound to 0
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &value);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // generate an empty renderbuffer, then get all parameters, they should equal to default values
    GLRenderbuffer renderBuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer);

    // Retrieve all values
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &value);
    ASSERT_EQ(value, 0);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &value);
    ASSERT_EQ(value, 0);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &value);
    ASSERT_EQ(value, GL_RGBA4);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_RED_SIZE, &value);
    ASSERT_EQ(value, 0);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_GREEN_SIZE, &value);
    ASSERT_EQ(value, 0);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_BLUE_SIZE, &value);
    ASSERT_EQ(value, 0);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_ALPHA_SIZE, &value);
    ASSERT_EQ(value, 0);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_DEPTH_SIZE, &value);
    ASSERT_EQ(value, 0);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_STENCIL_SIZE, &value);
    ASSERT_EQ(value, 0);

    // set buffer storage, then get all renderbuffer parameters

    // Allocate memory for renderbuffer
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 32, 32);

    /* Retrieve all values */
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &value);
    ASSERT_EQ(value, 32);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &value);
    ASSERT_EQ(value, 32);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &value);
    ASSERT_EQ(value, GL_DEPTH_COMPONENT16);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_RED_SIZE, &value);
    ASSERT_EQ(value, 0);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_GREEN_SIZE, &value);
    ASSERT_EQ(value, 0);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_BLUE_SIZE, &value);
    ASSERT_EQ(value, 0);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_ALPHA_SIZE, &value);
    ASSERT_EQ(value, 0);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_DEPTH_SIZE, &value);
    ASSERT_GE(value, 16);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_STENCIL_SIZE, &value);
    ASSERT_EQ(value, 0);

    // call glGetRenderbufferParameteriv with an invalid pname
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_INVALID_ENUM, &value);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Call with invalid target with invalid target
    glGetRenderbufferParameteriv(GL_INVALID_VALUE, GL_RENDERBUFFER_WIDTH, &value);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Test that passing NULL to the shader parameter in the glGetAttachedShaders function
// should return GL_INVALID_VALUE.
TEST_P(SimpleOperationTest, NullParameterInGetAttachedShaders)
{
    GLsizei length;
    GLuint attachedShaders[2] = {0u};
    GLuint program            = glCreateProgram();
    ASSERT_GL_NO_ERROR();

    glGetAttachedShaders(program, 0, &length, NULL);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glGetAttachedShaders(program, 0, NULL, attachedShaders);
    EXPECT_GL_ERROR(GL_NO_ERROR);
}

// Test that passing NULL separately to the range and precision parameters
// in the glGetShaderPrecisionFormat function should return GL_INVALID_VALUE.
TEST_P(SimpleOperationTest, NullParameterInGetShaderPrecisionFormat)
{
    GLint range[2], precision;

    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_LOW_FLOAT, range, NULL);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_LOW_FLOAT, NULL, &precision);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that glGetShaderPrecisionFormat only accepts GL_VERTEX_SHADER and GL_FRAGMENT_SHADER, and
// generates GL_INVALID_ENUM for other shader types, including GL_COMPUTE_SHADER.
TEST_P(SimpleOperationTest, InvalidShaderTypeInGetShaderPrecisionFormat)
{
    GLint range[2], precision;

    glGetShaderPrecisionFormat(GL_COMPUTE_SHADER, GL_LOW_FLOAT, range, &precision);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glGetShaderPrecisionFormat(GL_GEOMETRY_SHADER, GL_LOW_FLOAT, range, &precision);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3_AND(
    SimpleOperationTest,
    ES3_METAL().enable(Feature::ForceBufferGPUStorage),
    ES3_METAL().disable(Feature::HasExplicitMemBarrier).disable(Feature::HasCheapRenderPass),
    WithVulkanSecondaries(ES3_VULKAN_SWIFTSHADER()));

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3_AND(
    TriangleFanDrawTest,
    ES3_METAL().enable(Feature::ForceBufferGPUStorage),
    ES3_METAL().disable(Feature::HasExplicitMemBarrier).disable(Feature::HasCheapRenderPass),
    ES3_WEBGPU());

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3_AND(TriangleFanTest, ES3_WEBGPU());
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3_AND(WebGLTriangleFanTest, ES3_WEBGPU());

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3_AND_ES31(SimpleOperationTest31);

}  // namespace
