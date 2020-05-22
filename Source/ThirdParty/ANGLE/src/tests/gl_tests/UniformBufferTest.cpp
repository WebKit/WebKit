//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "util/random_utils.h"

using namespace angle;

namespace
{

class UniformBufferTest : public ANGLETest
{
  protected:
    UniformBufferTest()
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
        mkFS = R"(#version 300 es
precision highp float;
uniform uni { vec4 color; };
out vec4 fragColor;
void main()
{
    fragColor = color;
})";

        mProgram = CompileProgram(essl3_shaders::vs::Simple(), mkFS);
        ASSERT_NE(mProgram, 0u);

        mUniformBufferIndex = glGetUniformBlockIndex(mProgram, "uni");
        ASSERT_NE(mUniformBufferIndex, -1);

        glGenBuffers(1, &mUniformBuffer);

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteBuffers(1, &mUniformBuffer);
        glDeleteProgram(mProgram);
    }

    const char *mkFS;
    GLuint mProgram;
    GLint mUniformBufferIndex;
    GLuint mUniformBuffer;
};

// Basic UBO functionality.
TEST_P(UniformBufferTest, Simple)
{
    glClear(GL_COLOR_BUFFER_BIT);
    float floatData[4] = {0.5f, 0.75f, 0.25f, 1.0f};

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * 4, floatData, GL_STATIC_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);

    glUniformBlockBinding(mProgram, mUniformBufferIndex, 0);
    drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(0, 0, 128, 191, 64, 255, 1);
}

// Test that using a UBO with a non-zero offset and size actually works.
// The first step of this test renders a color from a UBO with a zero offset.
// The second step renders a color from a UBO with a non-zero offset.
TEST_P(UniformBufferTest, UniformBufferRange)
{
    int px = getWindowWidth() / 2;
    int py = getWindowHeight() / 2;

    // Query the uniform buffer alignment requirement
    GLint alignment;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);

    GLint64 maxUniformBlockSize;
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    if (alignment >= maxUniformBlockSize)
    {
        // ANGLE doesn't implement UBO offsets for this platform.
        // Ignore the test case.
        return;
    }

    ASSERT_GL_NO_ERROR();

    // Let's create a buffer which contains two vec4.
    GLuint vec4Size = 4 * sizeof(float);
    GLuint stride   = 0;
    do
    {
        stride += alignment;
    } while (stride < vec4Size);

    std::vector<char> v(2 * stride);
    float *first  = reinterpret_cast<float *>(v.data());
    float *second = reinterpret_cast<float *>(v.data() + stride);

    first[0] = 10.f / 255.f;
    first[1] = 20.f / 255.f;
    first[2] = 30.f / 255.f;
    first[3] = 40.f / 255.f;

    second[0] = 110.f / 255.f;
    second[1] = 120.f / 255.f;
    second[2] = 130.f / 255.f;
    second[3] = 140.f / 255.f;

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    // We use on purpose a size which is not a multiple of the alignment.
    glBufferData(GL_UNIFORM_BUFFER, stride + vec4Size, v.data(), GL_STATIC_DRAW);

    glUniformBlockBinding(mProgram, mUniformBufferIndex, 0);

    EXPECT_GL_NO_ERROR();

    // Bind the first part of the uniform buffer and draw
    // Use a size which is smaller than the alignment to check
    // to check that this case is handle correctly in the conversion to 11.1.
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, mUniformBuffer, 0, vec4Size);
    drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(px, py, 10, 20, 30, 40);

    // Bind the second part of the uniform buffer and draw
    // Furthermore the D3D11.1 backend will internally round the vec4Size (16 bytes) to a stride
    // (256 bytes) hence it will try to map the range [stride, 2 * stride] which is out-of-bound of
    // the buffer bufferSize = stride + vec4Size < 2 * stride. Ensure that this behaviour works.
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, mUniformBuffer, stride, vec4Size);
    drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(px, py, 110, 120, 130, 140);
}

// Test uniform block bindings.
TEST_P(UniformBufferTest, UniformBufferBindings)
{
    int px = getWindowWidth() / 2;
    int py = getWindowHeight() / 2;

    ASSERT_GL_NO_ERROR();

    // Let's create a buffer which contains one vec4.
    GLuint vec4Size = 4 * sizeof(float);
    std::vector<char> v(vec4Size);
    float *first = reinterpret_cast<float *>(v.data());

    first[0] = 10.f / 255.f;
    first[1] = 20.f / 255.f;
    first[2] = 30.f / 255.f;
    first[3] = 40.f / 255.f;

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, vec4Size, v.data(), GL_STATIC_DRAW);

    EXPECT_GL_NO_ERROR();

    // Try to bind the buffer to binding point 2
    glUniformBlockBinding(mProgram, mUniformBufferIndex, 2);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, mUniformBuffer);
    drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(px, py, 10, 20, 30, 40);

    // Clear the framebuffer
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(px, py, 0, 0, 0, 0);

    // Try to bind the buffer to another binding point
    glUniformBlockBinding(mProgram, mUniformBufferIndex, 5);
    glBindBufferBase(GL_UNIFORM_BUFFER, 5, mUniformBuffer);
    drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(px, py, 10, 20, 30, 40);
}

// Test that ANGLE handles used but unbound UBO. Assumes we are running on ANGLE and produce
// optional but not mandatory errors.
TEST_P(UniformBufferTest, ANGLEUnboundUniformBuffer)
{
    glUniformBlockBinding(mProgram, mUniformBufferIndex, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Update a UBO many time and verify that ANGLE uses the latest version of the data.
// https://code.google.com/p/angleproject/issues/detail?id=965
TEST_P(UniformBufferTest, UniformBufferManyUpdates)
{
    // TODO(jmadill): Figure out why this fails on OSX Intel OpenGL.
    ANGLE_SKIP_TEST_IF(IsIntel() && IsOSX() && IsOpenGL());

    int px = getWindowWidth() / 2;
    int py = getWindowHeight() / 2;

    ASSERT_GL_NO_ERROR();

    float data[4];

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(data), nullptr, GL_DYNAMIC_DRAW);
    glUniformBlockBinding(mProgram, mUniformBufferIndex, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);

    EXPECT_GL_NO_ERROR();

    // Repeteadly update the data and draw
    for (size_t i = 0; i < 10; ++i)
    {
        data[0] = (i + 10.f) / 255.f;
        data[1] = (i + 20.f) / 255.f;
        data[2] = (i + 30.f) / 255.f;
        data[3] = (i + 40.f) / 255.f;

        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(data), data);

        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_EQ(px, py, i + 10, i + 20, i + 30, i + 40);
    }
}

// Use a large number of buffer ranges (compared to the actual size of the UBO)
TEST_P(UniformBufferTest, ManyUniformBufferRange)
{
    int px = getWindowWidth() / 2;
    int py = getWindowHeight() / 2;

    // Query the uniform buffer alignment requirement
    GLint alignment;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);

    GLint64 maxUniformBlockSize;
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    if (alignment >= maxUniformBlockSize)
    {
        // ANGLE doesn't implement UBO offsets for this platform.
        // Ignore the test case.
        return;
    }

    ASSERT_GL_NO_ERROR();

    // Let's create a buffer which contains eight vec4.
    GLuint vec4Size = 4 * sizeof(float);
    GLuint stride   = 0;
    do
    {
        stride += alignment;
    } while (stride < vec4Size);

    std::vector<char> v(8 * stride);

    for (size_t i = 0; i < 8; ++i)
    {
        float *data = reinterpret_cast<float *>(v.data() + i * stride);

        data[0] = (i + 10.f) / 255.f;
        data[1] = (i + 20.f) / 255.f;
        data[2] = (i + 30.f) / 255.f;
        data[3] = (i + 40.f) / 255.f;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, v.size(), v.data(), GL_STATIC_DRAW);

    glUniformBlockBinding(mProgram, mUniformBufferIndex, 0);

    EXPECT_GL_NO_ERROR();

    // Bind each possible offset
    for (size_t i = 0; i < 8; ++i)
    {
        glBindBufferRange(GL_UNIFORM_BUFFER, 0, mUniformBuffer, i * stride, stride);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_EQ(px, py, 10 + i, 20 + i, 30 + i, 40 + i);
    }

    // Try to bind larger range
    for (size_t i = 0; i < 7; ++i)
    {
        glBindBufferRange(GL_UNIFORM_BUFFER, 0, mUniformBuffer, i * stride, 2 * stride);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_EQ(px, py, 10 + i, 20 + i, 30 + i, 40 + i);
    }

    // Try to bind even larger range
    for (size_t i = 0; i < 5; ++i)
    {
        glBindBufferRange(GL_UNIFORM_BUFFER, 0, mUniformBuffer, i * stride, 4 * stride);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_EQ(px, py, 10 + i, 20 + i, 30 + i, 40 + i);
    }
}

// Tests that active uniforms have the right names.
TEST_P(UniformBufferTest, ActiveUniformNames)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "in vec2 position;\n"
        "out vec2 v;\n"
        "uniform blockName1 {\n"
        "  float f1;\n"
        "} instanceName1;\n"
        "uniform blockName2 {\n"
        "  float f2;\n"
        "} instanceName2[1];\n"
        "void main() {\n"
        "  v = vec2(instanceName1.f1, instanceName2[0].f2);\n"
        "  gl_Position = vec4(position, 0, 1);\n"
        "}";

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "in vec2 v;\n"
        "out vec4 color;\n"
        "void main() {\n"
        "  color = vec4(v, 0, 1);\n"
        "}";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    GLint activeUniformBlocks;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &activeUniformBlocks);
    ASSERT_EQ(2, activeUniformBlocks);

    GLuint index = glGetUniformBlockIndex(program, "blockName1");
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();

    index = glGetUniformBlockIndex(program, "blockName2[0]");
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();

    GLint activeUniforms;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &activeUniforms);

    ASSERT_EQ(2, activeUniforms);

    GLint size;
    GLenum type;
    GLint maxLength;
    GLsizei length;

    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);
    std::vector<GLchar> strUniformNameBuffer(maxLength + 1, 0);
    const GLchar *uniformNames[1];
    uniformNames[0] = "blockName1.f1";
    glGetUniformIndices(program, 1, uniformNames, &index);
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();
    glGetActiveUniform(program, index, maxLength, &length, &size, &type, &strUniformNameBuffer[0]);
    EXPECT_EQ(1, size);
    EXPECT_GLENUM_EQ(GL_FLOAT, type);
    EXPECT_EQ("blockName1.f1", std::string(&strUniformNameBuffer[0]));

    uniformNames[0] = "blockName2.f2";
    glGetUniformIndices(program, 1, uniformNames, &index);
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();
    glGetActiveUniform(program, index, maxLength, &length, &size, &type, &strUniformNameBuffer[0]);
    EXPECT_EQ(1, size);
    EXPECT_GLENUM_EQ(GL_FLOAT, type);
    EXPECT_EQ("blockName2.f2", std::string(&strUniformNameBuffer[0]));
}

// Tests active uniforms and blocks when the layout is std140, shared and packed.
TEST_P(UniformBufferTest, ActiveUniformNumberAndName)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "in vec2 position;\n"
        "out float v;\n"
        "struct S {\n"
        "  highp ivec3 a;\n"
        "  mediump ivec2 b[4];\n"
        "};\n"
        "layout(std140) uniform blockName0 {\n"
        "  S s0;\n"
        "  lowp vec2 v0;\n"
        "  S s1[2];\n"
        "  highp uint u0;\n"
        "};\n"
        "layout(std140) uniform blockName1 {\n"
        "  float f1;\n"
        "  bool b1;\n"
        "} instanceName1;\n"
        "layout(shared) uniform blockName2 {\n"
        "  float f2;\n"
        "};\n"
        "layout(packed) uniform blockName3 {\n"
        "  float f3;\n"
        "};\n"
        "void main() {\n"
        "  v = instanceName1.f1;\n"
        "  gl_Position = vec4(position, 0, 1);\n"
        "}";

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "in float v;\n"
        "out vec4 color;\n"
        "void main() {\n"
        "  color = vec4(v, 0, 0, 1);\n"
        "}";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    // Note that the packed |blockName3| might (or might not) be optimized out.
    GLint activeUniforms;
    glGetProgramiv(program.get(), GL_ACTIVE_UNIFORMS, &activeUniforms);
    EXPECT_GE(activeUniforms, 11);

    GLint activeUniformBlocks;
    glGetProgramiv(program.get(), GL_ACTIVE_UNIFORM_BLOCKS, &activeUniformBlocks);
    EXPECT_GE(activeUniformBlocks, 3);

    GLint maxLength, size;
    GLenum type;
    GLsizei length;
    GLuint index;
    const GLchar *uniformNames[1];
    glGetProgramiv(program.get(), GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);
    std::vector<GLchar> strBuffer(maxLength + 1, 0);

    uniformNames[0] = "s0.a";
    glGetUniformIndices(program, 1, uniformNames, &index);
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();
    glGetActiveUniform(program.get(), index, maxLength, &length, &size, &type, &strBuffer[0]);
    EXPECT_EQ(1, size);
    EXPECT_EQ("s0.a", std::string(&strBuffer[0]));

    uniformNames[0] = "s0.b[0]";
    glGetUniformIndices(program, 1, uniformNames, &index);
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();
    glGetActiveUniform(program.get(), index, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(4, size);
    EXPECT_EQ("s0.b[0]", std::string(&strBuffer[0]));

    uniformNames[0] = "v0";
    glGetUniformIndices(program, 1, uniformNames, &index);
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();
    glGetActiveUniform(program.get(), index, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("v0", std::string(&strBuffer[0]));

    uniformNames[0] = "s1[0].a";
    glGetUniformIndices(program, 1, uniformNames, &index);
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();
    glGetActiveUniform(program.get(), index, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("s1[0].a", std::string(&strBuffer[0]));

    uniformNames[0] = "s1[0].b[0]";
    glGetUniformIndices(program, 1, uniformNames, &index);
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();
    glGetActiveUniform(program.get(), index, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(4, size);
    EXPECT_EQ("s1[0].b[0]", std::string(&strBuffer[0]));

    uniformNames[0] = "s1[1].a";
    glGetUniformIndices(program, 1, uniformNames, &index);
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();
    glGetActiveUniform(program.get(), index, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("s1[1].a", std::string(&strBuffer[0]));

    uniformNames[0] = "s1[1].b[0]";
    glGetUniformIndices(program, 1, uniformNames, &index);
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();
    glGetActiveUniform(program.get(), index, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(4, size);
    EXPECT_EQ("s1[1].b[0]", std::string(&strBuffer[0]));

    uniformNames[0] = "u0";
    glGetUniformIndices(program, 1, uniformNames, &index);
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();
    glGetActiveUniform(program.get(), index, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("u0", std::string(&strBuffer[0]));

    uniformNames[0] = "blockName1.f1";
    glGetUniformIndices(program, 1, uniformNames, &index);
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();
    glGetActiveUniform(program.get(), index, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("blockName1.f1", std::string(&strBuffer[0]));

    uniformNames[0] = "blockName1.b1";
    glGetUniformIndices(program, 1, uniformNames, &index);
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();
    glGetActiveUniform(program.get(), index, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("blockName1.b1", std::string(&strBuffer[0]));

    uniformNames[0] = "f2";
    glGetUniformIndices(program, 1, uniformNames, &index);
    EXPECT_NE(GL_INVALID_INDEX, index);
    ASSERT_GL_NO_ERROR();
    glGetActiveUniform(program.get(), index, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("f2", std::string(&strBuffer[0]));
}

// Test that using a very large buffer to back a small uniform block works OK.
TEST_P(UniformBufferTest, VeryLarge)
{
    glClear(GL_COLOR_BUFFER_BIT);
    float floatData[4] = {0.5f, 0.75f, 0.25f, 1.0f};

    GLsizei bigSize = 4096 * 64;
    std::vector<GLubyte> zero(bigSize, 0);

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, bigSize, zero.data(), GL_STATIC_DRAW);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(float) * 4, floatData);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);

    glUniformBlockBinding(mProgram, mUniformBufferIndex, 0);
    drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(0, 0, 128, 191, 64, 255, 1);
}

// Test that readback from a very large uniform buffer works OK.
TEST_P(UniformBufferTest, VeryLargeReadback)
{
    glClear(GL_COLOR_BUFFER_BIT);

    // Generate some random data.
    GLsizei bigSize = 4096 * 64;
    std::vector<GLubyte> expectedData(bigSize);
    for (GLsizei index = 0; index < bigSize; ++index)
    {
        expectedData[index] = static_cast<GLubyte>(index);
    }

    // Initialize the GL buffer.
    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, bigSize, expectedData.data(), GL_STATIC_DRAW);

    // Do a small update.
    GLsizei smallSize              = sizeof(float) * 4;
    std::array<float, 4> floatData = {{0.5f, 0.75f, 0.25f, 1.0f}};
    memcpy(expectedData.data(), floatData.data(), smallSize);

    glBufferSubData(GL_UNIFORM_BUFFER, 0, smallSize, expectedData.data());

    // Draw with the buffer.
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(mProgram, mUniformBufferIndex, 0);
    drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(0, 0, 128, 191, 64, 255, 1);

    // Read back the large buffer data.
    const void *mapPtr = glMapBufferRange(GL_UNIFORM_BUFFER, 0, bigSize, GL_MAP_READ_BIT);
    ASSERT_GL_NO_ERROR();
    const GLubyte *bytePtr = reinterpret_cast<const GLubyte *>(mapPtr);
    std::vector<GLubyte> actualData(bytePtr, bytePtr + bigSize);
    EXPECT_EQ(expectedData, actualData);

    glUnmapBuffer(GL_UNIFORM_BUFFER);
}

class UniformBufferTest31 : public ANGLETest
{
  protected:
    UniformBufferTest31()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Test uniform block bindings greater than GL_MAX_UNIFORM_BUFFER_BINDINGS cause compile error.
TEST_P(UniformBufferTest31, MaxUniformBufferBindingsExceeded)
{
    GLint maxUniformBufferBindings;
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxUniformBufferBindings);
    std::string source =
        "#version 310 es\n"
        "in vec4 position;\n"
        "layout(binding = ";
    std::stringstream ss;
    ss << maxUniformBufferBindings;
    source = source + ss.str() +
             ") uniform uni {\n"
             "    vec4 color;\n"
             "};\n"
             "void main()\n"
             "{\n"
             "    gl_Position = position;\n"
             "}";
    GLuint shader = CompileShader(GL_VERTEX_SHADER, source.c_str());
    EXPECT_EQ(0u, shader);
}

// Test uniform block bindings specified by layout in shader work properly.
TEST_P(UniformBufferTest31, UniformBufferBindings)
{
    constexpr char kVS[] =
        "#version 310 es\n"
        "in vec4 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = position;\n"
        "}";
    constexpr char kFS[] =
        "#version 310 es\n"
        "precision highp float;\n"
        "layout(binding = 2) uniform uni {\n"
        "    vec4 color;\n"
        "};\n"
        "out vec4 fragColor;\n"
        "void main()\n"
        "{"
        "    fragColor = color;\n"
        "}";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    GLuint uniformBufferIndex = glGetUniformBlockIndex(program, "uni");
    ASSERT_NE(GL_INVALID_INDEX, uniformBufferIndex);
    GLBuffer uniformBuffer;

    int px = getWindowWidth() / 2;
    int py = getWindowHeight() / 2;

    ASSERT_GL_NO_ERROR();

    // Let's create a buffer which contains one vec4.
    GLuint vec4Size = 4 * sizeof(float);
    std::vector<char> v(vec4Size);
    float *first = reinterpret_cast<float *>(v.data());

    first[0] = 10.f / 255.f;
    first[1] = 20.f / 255.f;
    first[2] = 30.f / 255.f;
    first[3] = 40.f / 255.f;

    glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffer.get());
    glBufferData(GL_UNIFORM_BUFFER, vec4Size, v.data(), GL_STATIC_DRAW);

    EXPECT_GL_NO_ERROR();

    glBindBufferBase(GL_UNIFORM_BUFFER, 2, uniformBuffer.get());
    drawQuad(program, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(px, py, 10, 20, 30, 40);

    // Clear the framebuffer
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(px, py, 0, 0, 0, 0);

    // Try to bind the buffer to another binding point
    glUniformBlockBinding(program, uniformBufferIndex, 5);
    glBindBufferBase(GL_UNIFORM_BUFFER, 5, uniformBuffer.get());
    drawQuad(program, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(px, py, 10, 20, 30, 40);
}

// Test uniform blocks used as instanced array take next binding point for each subsequent element.
TEST_P(UniformBufferTest31, ConsecutiveBindingsForBlockArray)
{
    constexpr char kFS[] =
        "#version 310 es\n"
        "precision highp float;\n"
        "layout(binding = 2) uniform uni {\n"
        "    vec4 color;\n"
        "} blocks[2];\n"
        "out vec4 fragColor;\n"
        "void main()\n"
        "{\n"
        "    fragColor = blocks[0].color + blocks[1].color;\n"
        "}";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    std::array<GLBuffer, 2> uniformBuffers;

    int px = getWindowWidth() / 2;
    int py = getWindowHeight() / 2;

    ASSERT_GL_NO_ERROR();

    // Let's create a buffer which contains one vec4.
    GLuint vec4Size = 4 * sizeof(float);
    std::vector<char> v(vec4Size);
    float *first = reinterpret_cast<float *>(v.data());

    first[0] = 10.f / 255.f;
    first[1] = 20.f / 255.f;
    first[2] = 30.f / 255.f;
    first[3] = 40.f / 255.f;

    glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffers[0].get());
    glBufferData(GL_UNIFORM_BUFFER, vec4Size, v.data(), GL_STATIC_DRAW);
    EXPECT_GL_NO_ERROR();
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, uniformBuffers[0].get());
    ASSERT_GL_NO_ERROR();
    glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffers[1].get());
    glBufferData(GL_UNIFORM_BUFFER, vec4Size, v.data(), GL_STATIC_DRAW);
    EXPECT_GL_NO_ERROR();
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, uniformBuffers[1].get());

    drawQuad(program, essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(px, py, 20, 40, 60, 80);
}

// Test the layout qualifier binding must be both specified(ESSL 3.10.4 section 9.2).
TEST_P(UniformBufferTest31, BindingMustBeBothSpecified)
{
    constexpr char kVS[] =
        "#version 310 es\n"
        "in vec4 position;\n"
        "uniform uni\n"
        "{\n"
        "    vec4 color;\n"
        "} block;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = position + block.color;\n"
        "}";
    constexpr char kFS[] =
        "#version 310 es\n"
        "precision highp float;\n"
        "layout(binding = 0) uniform uni\n"
        "{\n"
        "    vec4 color;\n"
        "} block;\n"
        "out vec4 fragColor;\n"
        "void main()\n"
        "{\n"
        "    fragColor = block.color;\n"
        "}";
    GLuint program = CompileProgram(kVS, kFS);
    ASSERT_EQ(0u, program);
}

// Test with a block containing an array of structs.
TEST_P(UniformBufferTest, BlockContainingArrayOfStructs)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 my_FragColor;\n"
        "struct light_t {\n"
        "    vec4 intensity;\n"
        "};\n"
        "const int maxLights = 2;\n"
        "layout(std140) uniform lightData { light_t lights[maxLights]; };\n"
        "vec4 processLight(vec4 lighting, light_t light)\n"
        "{\n"
        "    return lighting + light.intensity;\n"
        "}\n"
        "void main()\n"
        "{\n"
        "    vec4 lighting = vec4(0, 0, 0, 1);\n"
        "    for (int n = 0; n < maxLights; n++)\n"
        "    {\n"
        "        lighting = processLight(lighting, lights[n]);\n"
        "    }\n"
        "    my_FragColor = lighting;\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "lightData");

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    const GLsizei kStructCount        = 2;
    const GLsizei kVectorElementCount = 4;
    const GLsizei kBytesPerElement    = 4;
    const GLsizei kDataSize           = kStructCount * kVectorElementCount * kBytesPerElement;
    std::vector<GLubyte> v(kDataSize, 0);
    float *vAsFloat = reinterpret_cast<float *>(v.data());

    vAsFloat[1]                       = 0.5f;
    vAsFloat[kVectorElementCount + 1] = 0.5f;

    glBufferData(GL_UNIFORM_BUFFER, kDataSize, v.data(), GL_STATIC_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test with a block instance array containing an array of structs.
TEST_P(UniformBufferTest, BlockArrayContainingArrayOfStructs)
{
    constexpr char kFS[] =
        R"(#version 300 es

        precision highp float;
        out vec4 my_FragColor;
        struct light_t
        {
            vec4 intensity;
        };

        layout(std140) uniform lightData { light_t lights[2]; } buffers[2];

        vec4 processLight(vec4 lighting, light_t light)
        {
            return lighting + light.intensity;
        }
        void main()
        {
            vec4 lighting = vec4(0, 0, 0, 1);
            lighting = processLight(lighting, buffers[0].lights[0]);
            lighting = processLight(lighting, buffers[1].lights[1]);
            my_FragColor = lighting;
        })";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    GLint uniformBufferIndex  = glGetUniformBlockIndex(program, "lightData[0]");
    GLint uniformBuffer2Index = glGetUniformBlockIndex(program, "lightData[1]");

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    const GLsizei kStructCount        = 2;
    const GLsizei kVectorElementCount = 4;
    const GLsizei kBytesPerElement    = 4;
    const GLsizei kDataSize           = kStructCount * kVectorElementCount * kBytesPerElement;
    std::vector<GLubyte> v(kDataSize, 0);
    float *vAsFloat = reinterpret_cast<float *>(v.data());

    // In the first struct/vector of the first block
    vAsFloat[1] = 0.5f;

    glBufferData(GL_UNIFORM_BUFFER, kDataSize, v.data(), GL_STATIC_DRAW);

    GLBuffer uniformBuffer2;
    glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffer2);

    vAsFloat[1] = 0.0f;
    // In the second struct/vector of the second block
    vAsFloat[kVectorElementCount + 1] = 0.5f;
    glBufferData(GL_UNIFORM_BUFFER, kDataSize, v.data(), GL_STATIC_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, uniformBuffer2);
    glUniformBlockBinding(program, uniformBufferIndex, 0);
    glUniformBlockBinding(program, uniformBuffer2Index, 1);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test with a block containing an array of structs containing arrays.
TEST_P(UniformBufferTest, BlockContainingArrayOfStructsContainingArrays)
{
    constexpr char kFS[] =
        R"(#version 300 es
        precision highp float;
        out vec4 my_FragColor;
        struct light_t
        {
            vec4 intensity[3];
        };
        const int maxLights = 2;
        layout(std140) uniform lightData { light_t lights[maxLights]; };
        vec4 processLight(vec4 lighting, light_t light)
        {
            return lighting + light.intensity[1];
        }
        void main()
        {
            vec4 lighting = vec4(0, 0, 0, 1);
            lighting = processLight(lighting, lights[0]);
            lighting = processLight(lighting, lights[1]);
            my_FragColor = lighting;
        })";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "lightData");

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    const GLsizei kStructCount       = 2;
    const GLsizei kVectorsPerStruct  = 3;
    const GLsizei kElementsPerVector = 4;
    const GLsizei kBytesPerElement   = 4;
    const GLsizei kDataSize =
        kStructCount * kVectorsPerStruct * kElementsPerVector * kBytesPerElement;
    std::vector<GLubyte> v(kDataSize, 0);
    float *vAsFloat = reinterpret_cast<float *>(v.data());

    vAsFloat[kElementsPerVector + 1]                                          = 0.5f;
    vAsFloat[kVectorsPerStruct * kElementsPerVector + kElementsPerVector + 1] = 0.5f;

    glBufferData(GL_UNIFORM_BUFFER, kDataSize, v.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test with a block containing nested structs.
TEST_P(UniformBufferTest, BlockContainingNestedStructs)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 my_FragColor;\n"
        "struct light_t {\n"
        "    vec4 intensity;\n"
        "};\n"
        "struct lightWrapper_t {\n"
        "    light_t light;\n"
        "};\n"
        "const int maxLights = 2;\n"
        "layout(std140) uniform lightData { lightWrapper_t lightWrapper; };\n"
        "vec4 processLight(vec4 lighting, lightWrapper_t aLightWrapper)\n"
        "{\n"
        "    return lighting + aLightWrapper.light.intensity;\n"
        "}\n"
        "void main()\n"
        "{\n"
        "    vec4 lighting = vec4(0, 0, 0, 1);\n"
        "    for (int n = 0; n < maxLights; n++)\n"
        "    {\n"
        "        lighting = processLight(lighting, lightWrapper);\n"
        "    }\n"
        "    my_FragColor = lighting;\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "lightData");

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    const GLsizei kVectorsPerStruct  = 3;
    const GLsizei kElementsPerVector = 4;
    const GLsizei kBytesPerElement   = 4;
    const GLsizei kDataSize          = kVectorsPerStruct * kElementsPerVector * kBytesPerElement;
    std::vector<GLubyte> v(kDataSize, 0);
    float *vAsFloat = reinterpret_cast<float *>(v.data());

    vAsFloat[1] = 1.0f;

    glBufferData(GL_UNIFORM_BUFFER, kDataSize, v.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests GetUniformBlockIndex return value on error.
TEST_P(UniformBufferTest, GetUniformBlockIndexDefaultReturn)
{
    ASSERT_FALSE(glIsProgram(99));
    EXPECT_EQ(GL_INVALID_INDEX, glGetUniformBlockIndex(99, "farts"));
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Block names can be reserved names in GLSL, as long as they're not reserved in GLSL ES.
TEST_P(UniformBufferTest, UniformBlockReservedOpenGLName)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 my_FragColor;\n"
        "layout(std140) uniform buffer { vec4 color; };\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = color;\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "buffer");

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    const GLsizei kElementsPerVector = 4;
    const GLsizei kBytesPerElement   = 4;
    const GLsizei kDataSize          = kElementsPerVector * kBytesPerElement;
    std::vector<GLubyte> v(kDataSize, 0);
    float *vAsFloat = reinterpret_cast<float *>(v.data());

    vAsFloat[1] = 1.0f;
    vAsFloat[3] = 1.0f;

    glBufferData(GL_UNIFORM_BUFFER, kDataSize, v.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Block instance names can be reserved names in GLSL, as long as they're not reserved in GLSL ES.
TEST_P(UniformBufferTest, UniformBlockInstanceReservedOpenGLName)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 my_FragColor;\n"
        "layout(std140) uniform dmat2 { vec4 color; } buffer;\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = buffer.color;\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "dmat2");

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    const GLsizei kElementsPerVector = 4;
    const GLsizei kBytesPerElement   = 4;
    const GLsizei kDataSize          = kElementsPerVector * kBytesPerElement;
    std::vector<GLubyte> v(kDataSize, 0);
    float *vAsFloat = reinterpret_cast<float *>(v.data());

    vAsFloat[1] = 1.0f;
    vAsFloat[3] = 1.0f;

    glBufferData(GL_UNIFORM_BUFFER, kDataSize, v.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that uniform block instance with nested structs that contain vec3s inside is handled
// correctly. This is meant to test that HLSL structure padding to implement std140 layout works
// together with uniform blocks.
TEST_P(UniformBufferTest, Std140UniformBlockInstanceWithNestedStructsContainingVec3s)
{
    // Got incorrect test result on non-NVIDIA Android - the alpha channel was not set correctly
    // from the second vector, possibly the platform doesn't implement std140 packing right?
    // http://anglebug.com/2217
    ANGLE_SKIP_TEST_IF(IsAndroid() && !IsNVIDIA());

    constexpr char kFS[] =
        R"(#version 300 es

        precision highp float;
        out vec4 my_FragColor;

        struct Sinner {
          vec3 v;
        };

        struct S {
            Sinner s1;
            Sinner s2;
        };

        layout(std140) uniform structBuffer { S s; } buffer;

        void accessStruct(S s)
        {
            my_FragColor = vec4(s.s1.v.xy, s.s2.v.xy);
        }

        void main()
        {
            accessStruct(buffer.s);
        })";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "structBuffer");

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    const GLsizei kVectorsPerBlock         = 2;
    const GLsizei kElementsPerPaddedVector = 4;
    const GLsizei kBytesPerElement         = 4;
    const GLsizei kDataSize = kVectorsPerBlock * kElementsPerPaddedVector * kBytesPerElement;
    std::vector<GLubyte> v(kDataSize, 0);
    float *vAsFloat = reinterpret_cast<float *>(v.data());

    // Set second value in each vec3.
    vAsFloat[1u]      = 1.0f;
    vAsFloat[4u + 1u] = 1.0f;

    glBufferData(GL_UNIFORM_BUFFER, kDataSize, v.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests the detaching shaders from the program and using uniform blocks works.
// This covers a bug in ANGLE's D3D back-end.
TEST_P(UniformBufferTest, DetachShaders)
{
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, essl3_shaders::vs::Simple());
    ASSERT_NE(0u, vertexShader);
    GLuint kFS = CompileShader(GL_FRAGMENT_SHADER, mkFS);
    ASSERT_NE(0u, kFS);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, kFS);

    ASSERT_TRUE(LinkAttachedProgram(program));

    glDetachShader(program, vertexShader);
    glDetachShader(program, kFS);
    glDeleteShader(vertexShader);
    glDeleteShader(kFS);

    glClear(GL_COLOR_BUFFER_BIT);
    float floatData[4] = {0.5f, 0.75f, 0.25f, 1.0f};

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * 4, floatData, GL_STATIC_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);

    GLint uniformBufferIndex = glGetUniformBlockIndex(mProgram, "uni");
    ASSERT_NE(uniformBufferIndex, -1);

    glUniformBlockBinding(program, uniformBufferIndex, 0);
    drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(0, 0, 128, 191, 64, 255, 1);

    glDeleteProgram(program);
}

// Test a uniform block where the whole block is set as row-major.
TEST_P(UniformBufferTest, Std140UniformBlockWithRowMajorQualifier)
{
    // AMD OpenGL driver doesn't seem to apply the row-major qualifier right.
    // http://anglebug.com/2273
    ANGLE_SKIP_TEST_IF(IsAMD() && IsOpenGL() && !IsOSX());

    constexpr char kFS[] =
        R"(#version 300 es

        precision highp float;
        out vec4 my_FragColor;

        layout(std140, row_major) uniform matrixBuffer
        {
            mat2 m;
        } buffer;

        void main()
        {
            // Vector constructor accesses elements in column-major order.
            my_FragColor = vec4(buffer.m);
        })";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "matrixBuffer");

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    const GLsizei kElementsPerMatrix = 8;  // Each mat2 row gets padded into a vec4.
    const GLsizei kBytesPerElement   = 4;
    const GLsizei kDataSize          = kElementsPerMatrix * kBytesPerElement;
    std::vector<GLubyte> v(kDataSize, 0);
    float *vAsFloat = reinterpret_cast<float *>(v.data());

    vAsFloat[0u] = 1.0f;
    vAsFloat[1u] = 128.0f / 255.0f;
    vAsFloat[4u] = 64.0f / 255.0f;
    vAsFloat[5u] = 32.0f / 255.0f;

    glBufferData(GL_UNIFORM_BUFFER, kDataSize, v.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(255, 64, 128, 32), 5);
}

// Test a uniform block where an individual matrix field is set as row-major whereas the whole block
// is set as column-major.
TEST_P(UniformBufferTest, Std140UniformBlockWithPerMemberRowMajorQualifier)
{
    // AMD OpenGL driver doesn't seem to apply the row-major qualifier right.
    // http://anglebug.com/2273
    ANGLE_SKIP_TEST_IF(IsAMD() && IsOpenGL() && !IsOSX());

    constexpr char kFS[] =
        R"(#version 300 es

        precision highp float;
        out vec4 my_FragColor;

        layout(std140, column_major) uniform matrixBuffer
        {
            layout(row_major) mat2 m;
        } buffer;

        void main()
        {
            // Vector constructor accesses elements in column-major order.
            my_FragColor = vec4(buffer.m);
        })";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "matrixBuffer");

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    const GLsizei kElementsPerMatrix = 8;  // Each mat2 row gets padded into a vec4.
    const GLsizei kBytesPerElement   = 4;
    const GLsizei kDataSize          = kElementsPerMatrix * kBytesPerElement;
    std::vector<GLubyte> v(kDataSize, 0);
    float *vAsFloat = reinterpret_cast<float *>(v.data());

    vAsFloat[0u] = 1.0f;
    vAsFloat[1u] = 128.0f / 255.0f;
    vAsFloat[4u] = 64.0f / 255.0f;
    vAsFloat[5u] = 32.0f / 255.0f;

    glBufferData(GL_UNIFORM_BUFFER, kDataSize, v.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(255, 64, 128, 32), 5);
}

// Test a uniform block where an individual matrix field is set as column-major whereas the whole
// block is set as row-major.
TEST_P(UniformBufferTest, Std140UniformBlockWithPerMemberColumnMajorQualifier)
{
    constexpr char kFS[] =
        R"(#version 300 es

        precision highp float;
        out vec4 my_FragColor;

        layout(std140, row_major) uniform matrixBuffer
        {
            // 2 columns, 3 rows.
            layout(column_major) mat2x3 m;
        } buffer;

        void main()
        {
            // Vector constructor accesses elements in column-major order.
            my_FragColor = vec4(buffer.m);
        })";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "matrixBuffer");

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    const GLsizei kElementsPerMatrix = 8;  // Each mat2x3 column gets padded into a vec4.
    const GLsizei kBytesPerElement   = 4;
    const GLsizei kDataSize          = kElementsPerMatrix * kBytesPerElement;
    std::vector<GLubyte> v(kDataSize, 0);
    float *vAsFloat = reinterpret_cast<float *>(v.data());

    vAsFloat[0u] = 1.0f;
    vAsFloat[1u] = 192.0f / 255.0f;
    vAsFloat[2u] = 128.0f / 255.0f;
    vAsFloat[4u] = 96.0f / 255.0f;
    vAsFloat[5u] = 64.0f / 255.0f;
    vAsFloat[6u] = 32.0f / 255.0f;

    glBufferData(GL_UNIFORM_BUFFER, kDataSize, v.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(255, 192, 128, 96), 5);
}

// Test a uniform block where a struct field is set as row-major.
TEST_P(UniformBufferTest, Std140UniformBlockWithRowMajorQualifierOnStruct)
{
    // AMD OpenGL driver doesn't seem to apply the row-major qualifier right.
    // http://anglebug.com/2273
    ANGLE_SKIP_TEST_IF(IsAMD() && IsOpenGL() && !IsOSX());

    constexpr char kFS[] =
        R"(#version 300 es

        precision highp float;
        out vec4 my_FragColor;

        struct S
        {
            mat2 m;
        };

        layout(std140) uniform matrixBuffer
        {
            layout(row_major) S s;
        } buffer;

        void main()
        {
            // Vector constructor accesses elements in column-major order.
            my_FragColor = vec4(buffer.s.m);
        })";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "matrixBuffer");

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    const GLsizei kElementsPerMatrix = 8;  // Each mat2 row gets padded into a vec4.
    const GLsizei kBytesPerElement   = 4;
    const GLsizei kDataSize          = kElementsPerMatrix * kBytesPerElement;
    std::vector<GLubyte> v(kDataSize, 0);
    float *vAsFloat = reinterpret_cast<float *>(v.data());

    vAsFloat[0u] = 1.0f;
    vAsFloat[1u] = 128.0f / 255.0f;
    vAsFloat[4u] = 64.0f / 255.0f;
    vAsFloat[5u] = 32.0f / 255.0f;

    glBufferData(GL_UNIFORM_BUFFER, kDataSize, v.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(255, 64, 128, 32), 5);
}

constexpr char kFragmentShader[] = R"(#version 300 es
precision mediump float;

layout (std140) uniform color_ubo
{
  vec4 color;
};

out vec4 fragColor;
void main()
{
  fragColor = color;
})";

// Regression test for a dirty bit bug in ANGLE. See http://crbug.com/792966
TEST_P(UniformBufferTest, SimpleBindingChange)
{
    // http://anglebug.com/2287
    ANGLE_SKIP_TEST_IF(IsOSX() && IsNVIDIA() && IsDesktopOpenGL());

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFragmentShader);

    glBindAttribLocation(program, 0, essl3_shaders::PositionAttrib());
    glUseProgram(program);
    GLint uboIndex = glGetUniformBlockIndex(program, "color_ubo");

    std::array<GLfloat, 12> vertices{{-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0}};
    GLBuffer vertexBuf;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuf);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);

    std::array<GLshort, 12> indexData = {{0, 1, 2, 2, 1, 3, 0, 1, 2, 2, 1, 3}};

    GLBuffer indexBuf;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexData.size() * sizeof(GLshort), indexData.data(),
                 GL_STATIC_DRAW);

    // Bind a first buffer with red.
    GLBuffer uboBuf1;
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboBuf1);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GLColor32F), &kFloatRed, GL_STATIC_DRAW);
    glUniformBlockBinding(program, uboIndex, 0);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    // Bind a second buffer with green, updating the buffer binding.
    GLBuffer uboBuf2;
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, uboBuf2);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GLColor32F), &kFloatGreen, GL_STATIC_DRAW);
    glUniformBlockBinding(program, uboIndex, 1);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, reinterpret_cast<const GLvoid *>(12));

    // Verify we get the second buffer.
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Regression test for a dirty bit bug in ANGLE. Same as above but for the indexed bindings.
TEST_P(UniformBufferTest, SimpleBufferChange)
{
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFragmentShader);

    glBindAttribLocation(program, 0, essl3_shaders::PositionAttrib());
    glUseProgram(program);
    GLint uboIndex = glGetUniformBlockIndex(program, "color_ubo");

    std::array<GLfloat, 12> vertices{{-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0}};
    GLBuffer vertexBuf;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuf);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);

    std::array<GLshort, 12> indexData = {{0, 1, 2, 2, 1, 3, 0, 1, 2, 2, 1, 3}};

    GLBuffer indexBuf;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexData.size() * sizeof(GLshort), indexData.data(),
                 GL_STATIC_DRAW);

    // Bind a first buffer with red.
    GLBuffer uboBuf1;
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboBuf1);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GLColor32F), &kFloatRed, GL_STATIC_DRAW);
    glUniformBlockBinding(program, uboIndex, 0);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    // Bind a second buffer to the same binding point (0). This should set to draw green.
    GLBuffer uboBuf2;
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboBuf2);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GLColor32F), &kFloatGreen, GL_STATIC_DRAW);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, reinterpret_cast<const GLvoid *>(12));

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests a bug in the D3D11 back-end where re-creating the buffer storage should trigger a state
// update in the State Manager class.
TEST_P(UniformBufferTest, DependentBufferChange)
{
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFragmentShader);

    glBindAttribLocation(program, 0, essl3_shaders::PositionAttrib());
    glUseProgram(program);
    GLint uboIndex = glGetUniformBlockIndex(program, "color_ubo");

    std::array<GLfloat, 12> vertices{{-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0}};
    GLBuffer vertexBuf;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuf);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);

    std::array<GLshort, 6> indexData = {{0, 1, 2, 2, 1, 3}};

    GLBuffer indexBuf;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexData.size() * sizeof(GLshort), indexData.data(),
                 GL_STATIC_DRAW);

    GLBuffer ubo;
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GLColor32F), &kFloatRed, GL_STATIC_DRAW);
    glUniformBlockBinding(program, uboIndex, 0);

    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, 0);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Resize the buffer - triggers a re-allocation in the D3D11 back-end.
    std::vector<GLColor32F> bigData(128, kFloatGreen);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GLColor32F) * bigData.size(), bigData.data(),
                 GL_STATIC_DRAW);

    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, 0);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Recreate WebGL conformance test conformance2/uniforms/large-uniform-buffers.html to test
// regression in http://anglebug.com/3388
TEST_P(UniformBufferTest, SizeOverMaxBlockSize)
{
    // Test crashes on Windows AMD OpenGL
    ANGLE_SKIP_TEST_IF(IsAMD() && IsWindows() && IsOpenGL());

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFragmentShader);

    glBindAttribLocation(program, 0, essl3_shaders::PositionAttrib());
    glUseProgram(program);
    GLint uboIndex = glGetUniformBlockIndex(program, "color_ubo");

    std::array<GLfloat, 12> vertices{{-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0}};
    GLBuffer vertexBuf;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuf);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);

    std::array<GLshort, 6> indexData = {{0, 1, 2, 2, 1, 3}};

    GLBuffer indexBuf;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexData.size() * sizeof(GLshort), indexData.data(),
                 GL_STATIC_DRAW);

    GLint uboDataSize = 0;
    glGetActiveUniformBlockiv(program, uboIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &uboDataSize);
    EXPECT_NE(uboDataSize, 0);  // uniform block data size invalid

    GLint64 maxUniformBlockSize;
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);

    GLBuffer uboBuf;
    std::vector<GLfloat> uboData;
    uboData.resize(maxUniformBlockSize * 2);  // underlying data is twice the max block size

    GLint offs0 = 0;

    // Red
    uboData[offs0 + 0] = 1;
    uboData[offs0 + 1] = 0;
    uboData[offs0 + 2] = 0;
    uboData[offs0 + 3] = 1;

    GLint offs1     = maxUniformBlockSize;
    GLint alignment = 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
    EXPECT_EQ(offs1 % alignment, 0);

    // Green
    uboData[offs1 + 0] = 0;
    uboData[offs1 + 1] = 1;
    uboData[offs1 + 2] = 0;
    uboData[offs1 + 3] = 1;

    glUniformBlockBinding(program, uboIndex, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboBuf);
    glBufferData(GL_UNIFORM_BUFFER, uboData.size() * sizeof(GLfloat), uboData.data(),
                 GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();  // No errors from setup

    // Draw lower triangle - should be red
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuf, offs0 * sizeof(float), 4 * sizeof(float));
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, 0);
    ASSERT_GL_NO_ERROR();  // No errors from draw

    // Draw upper triangle - should be green
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboBuf, offs1 * sizeof(float), 4 * sizeof(float));
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT,
                   reinterpret_cast<void *>(3 * sizeof(GLshort)));
    ASSERT_GL_NO_ERROR();  // No errors from draw

    GLint width  = getWindowWidth();
    GLint height = getWindowHeight();
    // Lower left should be red
    EXPECT_PIXEL_COLOR_EQ(width / 2 - 5, height / 2 - 5, GLColor::red);
    // Top right should be green
    EXPECT_PIXEL_COLOR_EQ(width / 2 + 5, height / 2 + 5, GLColor::green);
}

// Compile uniform buffer with large array member.
TEST_P(UniformBufferTest, LargeArrayOfStructs)
{
    constexpr char kVertexShader[] = R"(#version 300 es
        struct InstancingData
        {
            mat4 transformation;
        };

        #define MAX_INSTANCE_COUNT 800

        layout(std140) uniform InstanceBlock
        {
            InstancingData instances[MAX_INSTANCE_COUNT];
        };

        void main()
        {
            gl_Position = vec4(1.0) * instances[gl_InstanceID].transformation;
        })";

    constexpr char kFragmentShader[] = R"(#version 300 es
        precision mediump float;
        out vec4 outFragColor;
        void main()
        {
            outFragColor = vec4(0.0);
        })";

    ANGLE_GL_PROGRAM(program, kVertexShader, kFragmentShader);
    // Add a draw call for the sake of the Vulkan backend that currently really builds shaders at
    // draw time.
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
}

// Test uniform buffer with large struct array member, where the struct itself contains a mat4
// member.
TEST_P(UniformBufferTest, UniformBlockWithOneLargeStructArray)
{
    GLint64 maxUniformBlockSize;
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    std::ostringstream stream;
    GLuint arraySize;
    // Ensure that shader uniform block do not exceed MAX_UNIFORM_BLOCK_SIZE limit.
    if (maxUniformBlockSize >= 16384 && maxUniformBlockSize < 32768)
    {
        arraySize = 128;
        stream << "const uint arraySize = 128u;\n"
                  "const uint divisor1 = 128u;\n"
                  "const uint divisor2 = 32u;\n";
    }
    else if (maxUniformBlockSize >= 32768 && maxUniformBlockSize < 65536)
    {
        arraySize = 256;
        stream << "const uint arraySize = 256u;\n"
                  "const uint divisor1 = 64u;\n"
                  "const uint divisor2 = 16u;\n";
    }
    else
    {
        arraySize = 512;
        stream << "const uint arraySize = 512u;\n"
                  "const uint divisor1 = 32u;\n"
                  "const uint divisor2 = 8u;\n";
    }

    const std::string &kFS =
        "#version 300 es\n"
        "precision highp float;\n" +
        stream.str() +
        "out vec4 my_FragColor;\n"
        "struct S { mat4 color;};\n"
        "layout(std140) uniform buffer { S s[arraySize]; };\n"
        "void main()\n"
        "{\n"
        "    uvec2 coord = uvec2(floor(gl_FragCoord.xy));\n"
        "    uint index = coord.x +  coord.y * 128u;\n"
        "    uint index_x = index / divisor1;\n"
        "    uint index_y = (index % divisor1) / divisor2;\n"
        "    my_FragColor = s[index_x].color[index_y];\n"
        "}\n";

    GLint blockSize;
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS.c_str());
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "buffer");
    glGetActiveUniformBlockiv(program, uniformBufferIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, blockSize, nullptr, GL_STATIC_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);

    const GLuint kVectorPerMat   = 4;
    const GLuint kFloatPerVector = 4;
    GLuint kVectorCount          = arraySize * kVectorPerMat;
    GLuint kFloatCount           = kVectorCount * kFloatPerVector;
    std::vector<GLfloat> floatData(kFloatCount, 0.0f);
    const GLuint kPositionCount                    = 12;
    unsigned int positionToTest[kPositionCount][2] = {{0, 0},  {75, 0},  {98, 13},  {31, 31},
                                                      {0, 32}, {65, 33}, {23, 54},  {63, 63},
                                                      {0, 64}, {43, 86}, {53, 100}, {127, 127}};

    for (GLuint i = 0; i < kVectorCount; i++)
    {
        floatData[4 * i + 2] = 1.0f;
        floatData[4 * i + 3] = 1.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::blue);
    }

    for (GLuint i = 0; i < kVectorCount; i++)
    {
        floatData[4 * i + 1] = 1.0f;
        floatData[4 * i + 2] = 0.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::green);
    }

    for (GLuint i = kVectorCount / 4; i < kVectorCount / 2; i++)
    {
        floatData[4 * i]     = 1.0f;
        floatData[4 * i + 1] = 0.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        if (positionToTest[i][1] > 31 && positionToTest[i][1] < 64)
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::red);
        }
        else
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::green);
        }
    }
}

// Test instance uniform buffer with large struct array member, where the struct
// itself contains a mat4 member.
TEST_P(UniformBufferTest, InstanceUniformBlockWithOneLargeStructArray)
{
    GLint64 maxUniformBlockSize;
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    std::ostringstream stream;
    GLuint arraySize;
    // Ensure that shader uniform block do not exceed MAX_UNIFORM_BLOCK_SIZE limit.
    if (maxUniformBlockSize >= 16384 && maxUniformBlockSize < 32768)
    {
        arraySize = 128;
        stream << "const uint arraySize = 128u;\n"
                  "const uint divisor1 = 128u;\n"
                  "const uint divisor2 = 32u;\n";
    }
    else if (maxUniformBlockSize >= 32768 && maxUniformBlockSize < 65536)
    {
        arraySize = 256;
        stream << "const uint arraySize = 256u;\n"
                  "const uint divisor1 = 64u;\n"
                  "const uint divisor2 = 16u;\n";
    }
    else
    {
        arraySize = 512;
        stream << "const uint arraySize = 512u;\n"
                  "const uint divisor1 = 32u;\n"
                  "const uint divisor2 = 8u;\n";
    }

    const std::string &kFS =
        "#version 300 es\n"
        "precision highp float;\n" +
        stream.str() +
        "out vec4 my_FragColor;\n"
        "struct S { mat4 color;};\n"
        "layout(std140) uniform buffer { S s[arraySize]; } instance;\n"
        "void main()\n"
        "{\n"
        "    uvec2 coord = uvec2(floor(gl_FragCoord.xy));\n"
        "    uint index = coord.x +  coord.y * 128u;\n"
        "    uint index_x = index / divisor1;\n"
        "    uint index_y = (index % divisor1) / divisor2;\n"
        "    my_FragColor = instance.s[index_x].color[index_y];\n"
        "}\n";

    GLint blockSize;
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS.c_str());
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "buffer");
    glGetActiveUniformBlockiv(program, uniformBufferIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, blockSize, nullptr, GL_STATIC_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);

    const GLuint kVectorPerMat   = 4;
    const GLuint kFloatPerVector = 4;
    GLuint kVectorCount          = arraySize * kVectorPerMat;
    GLuint kFloatCount           = kVectorCount * kFloatPerVector;
    std::vector<GLfloat> floatData(kFloatCount, 0.0f);
    const GLuint kPositionCount                    = 12;
    unsigned int positionToTest[kPositionCount][2] = {{0, 0},  {75, 0},  {98, 13},  {31, 31},
                                                      {0, 32}, {65, 33}, {23, 54},  {63, 63},
                                                      {0, 64}, {43, 86}, {53, 100}, {127, 127}};

    for (GLuint i = 0; i < kVectorCount; i++)
    {
        floatData[4 * i + 2] = 1.0f;
        floatData[4 * i + 3] = 1.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::blue);
    }

    for (GLuint i = 0; i < kVectorCount; i++)
    {
        floatData[4 * i + 1] = 1.0f;
        floatData[4 * i + 2] = 0.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::green);
    }

    for (GLuint i = kVectorCount / 4; i < kVectorCount / 2; i++)
    {
        floatData[4 * i]     = 1.0f;
        floatData[4 * i + 1] = 0.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        if (positionToTest[i][1] > 31 && positionToTest[i][1] < 64)
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::red);
        }
        else
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::green);
        }
    }
}

// Test instance array uniform buffer with large struct array member, where the
// struct itself contains a mat4 member.
TEST_P(UniformBufferTest, InstanceArrayUniformBlockWithOneLargeStructArray)
{
    GLint64 maxUniformBlockSize;
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    std::ostringstream stream;
    GLuint arraySize;
    // Ensure that shader uniform block do not exceed MAX_UNIFORM_BLOCK_SIZE limit.
    if (maxUniformBlockSize >= 16384 && maxUniformBlockSize < 32768)
    {
        arraySize = 128;
        stream << "const uint arraySize = 128u;\n"
                  "const uint divisor1 = 128u;\n"
                  "const uint divisor2 = 32u;\n";
    }
    else if (maxUniformBlockSize >= 32768 && maxUniformBlockSize < 65536)
    {
        arraySize = 256;
        stream << "const uint arraySize = 256u;\n"
                  "const uint divisor1 = 64u;\n"
                  "const uint divisor2 = 16u;\n";
    }
    else
    {
        arraySize = 512;
        stream << "const uint arraySize = 512u;\n"
                  "const uint divisor1 = 32u;\n"
                  "const uint divisor2 = 8u;\n";
    }

    const std::string &kFS =
        "#version 300 es\n"
        "precision highp float;\n" +
        stream.str() +
        "out vec4 my_FragColor;\n"
        "struct S { mat4 color;};\n"
        "layout(std140) uniform buffer { S s[arraySize]; } instance[2];\n"
        "void main()\n"
        "{\n"
        "    uvec2 coord = uvec2(floor(gl_FragCoord.xy));\n"
        "    uint index = coord.x +  coord.y * 128u;\n"
        "    uint index_x = index / divisor1;\n"
        "    uint index_y = (index % divisor1) / divisor2;\n"
        "    my_FragColor = instance[0].s[index_x].color[index_y] + "
        "instance[1].s[index_x].color[index_y];\n"
        "}\n";

    GLint blockSize0, blockSize1;
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS.c_str());
    GLint uniformBufferIndex0 = glGetUniformBlockIndex(program, "buffer[0]");
    GLint uniformBufferIndex1 = glGetUniformBlockIndex(program, "buffer[1]");
    glGetActiveUniformBlockiv(program, uniformBufferIndex0, GL_UNIFORM_BLOCK_DATA_SIZE,
                              &blockSize0);
    glGetActiveUniformBlockiv(program, uniformBufferIndex1, GL_UNIFORM_BLOCK_DATA_SIZE,
                              &blockSize1);

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, blockSize0, nullptr, GL_STATIC_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex0, 0);

    const GLuint kVectorPerMat   = 4;
    const GLuint kFloatPerVector = 4;
    GLuint kVectorCount          = arraySize * kVectorPerMat;
    GLuint kFloatCount           = kVectorCount * kFloatPerVector;
    std::vector<GLfloat> floatData0(kFloatCount, 0.0f);
    std::vector<GLfloat> floatData1(kFloatCount, 0.0f);
    const GLuint kPositionCount                    = 12;
    unsigned int positionToTest[kPositionCount][2] = {{0, 0},  {75, 0},  {98, 13},  {31, 31},
                                                      {0, 32}, {65, 33}, {23, 54},  {63, 63},
                                                      {0, 64}, {43, 86}, {53, 100}, {127, 127}};

    for (GLuint i = 0; i < kVectorCount; i++)
    {
        floatData0[4 * i + 2] = 1.0f;
        floatData0[4 * i + 3] = 1.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData0.data());

    GLBuffer uniformBuffer1;
    glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffer1);
    glBufferData(GL_UNIFORM_BUFFER, blockSize1, nullptr, GL_STATIC_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, 1, uniformBuffer1);
    glUniformBlockBinding(program, uniformBufferIndex0, 1);

    for (GLuint i = 0; i < kVectorCount; i++)
    {
        floatData1[4 * i + 1] = 1.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData1.data());

    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::cyan);
    }

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    for (GLuint i = 0; i < kVectorCount; i++)
    {
        floatData0[4 * i]     = 1.0f;
        floatData0[4 * i + 2] = 0.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData0.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::yellow);
    }

    glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffer1);
    for (GLuint i = kVectorCount / 4; i < kVectorCount / 2; i++)
    {
        floatData1[4 * i + 2] = 1.0f;
        floatData1[4 * i + 1] = 0.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData1.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        if (positionToTest[i][1] > 31 && positionToTest[i][1] < 64)
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::magenta);
        }
        else
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::yellow);
        }
    }
}

// Test uniform buffer with large struct array member, where the struct itself contains
// a mat4 member and a float member.
TEST_P(UniformBufferTest, UniformBlockWithOneLargeMixStructArray)
{
    GLint64 maxUniformBlockSize;
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    std::ostringstream stream;
    GLuint arraySize;
    // Ensure that shader uniform block do not exceed MAX_UNIFORM_BLOCK_SIZE limit.
    if (maxUniformBlockSize >= 16384 && maxUniformBlockSize < 32768)
    {
        arraySize = 128;
        stream << "const uint arraySize = 128u;\n"
                  "const uint divisor1 = 128u;\n"
                  "const uint divisor2 = 32u;\n";
    }
    else if (maxUniformBlockSize >= 32768 && maxUniformBlockSize < 65536)
    {
        arraySize = 256;
        stream << "const uint arraySize = 256u;\n"
                  "const uint divisor1 = 64u;\n"
                  "const uint divisor2 = 16u;\n";
    }
    else
    {
        arraySize = 512;
        stream << "const uint arraySize = 512u;\n"
                  "const uint divisor1 = 32u;\n"
                  "const uint divisor2 = 8u;\n";
    }

    const std::string &kFS =
        "#version 300 es\n"
        "precision highp float;\n" +
        stream.str() +
        "out vec4 my_FragColor;\n"
        "struct S { mat4 color; float factor; };\n"
        "layout(std140) uniform buffer { S s[arraySize]; };\n"
        "void main()\n"
        "{\n"
        "    uvec2 coord = uvec2(floor(gl_FragCoord.xy));\n"
        "    uint index = coord.x +  coord.y * 128u;\n"
        "    uint index_x = index / divisor1;\n"
        "    uint index_y = (index % divisor1) / divisor2;\n"
        "    my_FragColor = s[index_x].factor * s[index_x].color[index_y];\n"
        "}\n";

    GLint blockSize;
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS.c_str());
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "buffer");
    glGetActiveUniformBlockiv(program, uniformBufferIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, blockSize, nullptr, GL_STATIC_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);

    const GLuint kVectorPerMat   = 4;
    const GLuint kFloatPerVector = 4;
    // The member s is an array of S structures, each element of s should be rounded up
    // to the base alignment of a vec4 according to std140 storage layout rules.
    GLuint kFloatCount = arraySize * (kVectorPerMat * kFloatPerVector + kFloatPerVector);
    std::vector<GLfloat> floatData(kFloatCount, 0.0f);
    const GLuint kPositionCount                    = 12;
    unsigned int positionToTest[kPositionCount][2] = {{0, 0},  {75, 0},  {98, 13},  {31, 31},
                                                      {0, 32}, {65, 33}, {23, 54},  {63, 63},
                                                      {0, 64}, {43, 86}, {53, 100}, {127, 127}};

    const size_t kStrideofFloatCount = kVectorPerMat * kFloatPerVector + kFloatPerVector;
    for (GLuint i = 0; i < arraySize; i++)
    {
        for (GLuint j = 0; j < kVectorPerMat; j++)
        {
            floatData[i * kStrideofFloatCount + kVectorPerMat * j + 2] = 0.5f;
            floatData[i * kStrideofFloatCount + kVectorPerMat * j + 3] = 0.5f;
        }
        floatData[i * kStrideofFloatCount + kVectorPerMat * kFloatPerVector] = 2.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0,
                    std::min(static_cast<size_t>(blockSize), kFloatCount * sizeof(GLfloat)),
                    floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::blue);
    }

    for (GLuint i = 0; i < arraySize; i++)
    {
        for (GLuint j = 0; j < kVectorPerMat; j++)
        {
            floatData[i * kStrideofFloatCount + kVectorPerMat * j + 1] = 0.5f;
            floatData[i * kStrideofFloatCount + kVectorPerMat * j + 2] = 0.0f;
        }
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0,
                    std::min(static_cast<size_t>(blockSize), kFloatCount * sizeof(GLfloat)),
                    floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::green);
    }

    for (GLuint i = arraySize / 4; i < arraySize / 2; i++)
    {
        for (GLuint j = 0; j < kVectorPerMat; j++)
        {
            floatData[i * kStrideofFloatCount + kVectorPerMat * j]     = 0.5f;
            floatData[i * kStrideofFloatCount + kVectorPerMat * j + 1] = 0.0f;
        }
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0,
                    std::min(static_cast<size_t>(blockSize), kFloatCount * sizeof(GLfloat)),
                    floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        if (positionToTest[i][1] > 31 && positionToTest[i][1] < 64)
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::red);
        }
        else
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::green);
        }
    }
}

// Test a uniform block with large struct array member and a uniform block with small
// struct array member in the same program, and they share a uniform buffer.
TEST_P(UniformBufferTest, UniformBlocksInSameProgramShareUniformBuffer)
{
    GLint64 maxUniformBlockSize;
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    std::ostringstream stream;
    GLuint arraySize1, arraySize2;
    // Ensure that shader uniform block do not exceed MAX_UNIFORM_BLOCK_SIZE limit.
    if (maxUniformBlockSize >= 16384 && maxUniformBlockSize < 32768)
    {
        arraySize1 = 128;
        arraySize2 = 8;
        stream << "const uint arraySize1 = 128u;\n"
                  "const uint arraySize2 = 8u;\n"
                  "const uint divisor1 = 128u;\n"
                  "const uint divisor2 = 32u;\n"
                  "const uint divisor3 = 16u;\n";
    }
    else if (maxUniformBlockSize >= 32768 && maxUniformBlockSize < 65536)
    {
        arraySize1 = 256;
        arraySize2 = 16;
        stream << "const uint arraySize1 = 256u;\n"
                  "const uint arraySize2 = 16u;\n"
                  "const uint divisor1 = 64u;\n"
                  "const uint divisor2 = 16u;\n"
                  "const uint divisor3 = 8u;\n";
    }
    else
    {
        arraySize1 = 512;
        arraySize2 = 32;
        stream << "const uint arraySize1 = 512u;\n"
                  "const uint arraySize2 = 32u;\n"
                  "const uint divisor1 = 32u;\n"
                  "const uint divisor2 = 8u;\n"
                  "const uint divisor3 = 4u;\n";
    }

    const std::string &kFS =
        "#version 300 es\n"
        "precision highp float;\n" +
        stream.str() +
        "out vec4 my_FragColor;\n"
        "struct S { mat4 color;};\n"
        "layout(std140) uniform buffer1 { S s1[arraySize1]; };\n"
        "layout(std140) uniform buffer2 { S s2[arraySize2]; };\n"
        "void main()\n"
        "{\n"
        "    uvec2 coord = uvec2(floor(gl_FragCoord.xy));\n"
        "    uint index = coord.x +  coord.y * 128u;\n"
        "    uint index_x1 = index / divisor1;\n"
        "    uint index_y1 = (index % divisor1) / divisor2;\n"
        "    uint index_x2 = coord.x / divisor3;\n"
        "    uint index_y2 = coord.x % 4u;\n"
        "    my_FragColor = s1[index_x1].color[index_y1] + s2[index_x2].color[index_y2];\n"
        "}\n";

    GLint blockSize1, blockSize2;
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS.c_str());
    GLint uniformBufferIndex1 = glGetUniformBlockIndex(program, "buffer1");
    GLint uniformBufferIndex2 = glGetUniformBlockIndex(program, "buffer2");
    glGetActiveUniformBlockiv(program, uniformBufferIndex1, GL_UNIFORM_BLOCK_DATA_SIZE,
                              &blockSize1);
    glGetActiveUniformBlockiv(program, uniformBufferIndex2, GL_UNIFORM_BLOCK_DATA_SIZE,
                              &blockSize2);

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, blockSize1 + blockSize2, nullptr, GL_STATIC_DRAW);

    glBindBufferRange(GL_UNIFORM_BUFFER, 0, mUniformBuffer, 0, blockSize2);
    glUniformBlockBinding(program, uniformBufferIndex2, 0);
    glBindBufferRange(GL_UNIFORM_BUFFER, 1, mUniformBuffer, blockSize2, blockSize1);
    glUniformBlockBinding(program, uniformBufferIndex1, 1);

    const GLuint kVectorPerMat   = 4;
    const GLuint kFloatPerVector = 4;
    GLuint kVectorCount1         = arraySize1 * kVectorPerMat;
    GLuint kVectorCount2         = arraySize2 * kVectorPerMat;
    GLuint kFloatCount1          = kVectorCount1 * kFloatPerVector;
    GLuint kFloatCount2          = kVectorCount2 * kFloatPerVector;
    GLuint kFloatCount           = kFloatCount1 + kFloatCount2;
    std::vector<GLfloat> floatData(kFloatCount, 0.0f);
    const GLuint kPositionCount                    = 12;
    unsigned int positionToTest[kPositionCount][2] = {{0, 0},  {75, 0},  {98, 13},  {31, 31},
                                                      {0, 32}, {65, 33}, {23, 54},  {63, 63},
                                                      {0, 64}, {43, 86}, {53, 100}, {127, 127}};

    for (GLuint i = kVectorCount2; i < kVectorCount1 + kVectorCount2; i++)
    {
        floatData[4 * i + 2] = 1.0f;
        floatData[4 * i + 3] = 1.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount2 * sizeof(GLfloat), &floatData[0]);
    glBufferSubData(GL_UNIFORM_BUFFER, blockSize2, kFloatCount1 * sizeof(GLfloat),
                    &floatData[kFloatCount2]);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::blue);
    }

    for (GLuint i = 0; i < kVectorCount2; i++)
    {
        floatData[4 * i + 1] = 1.0f;
    }
    for (GLuint i = kVectorCount2 + kVectorCount1 / 4; i < kVectorCount2 + kVectorCount1 / 2; i++)
    {
        floatData[4 * i]     = 1.0f;
        floatData[4 * i + 2] = 0.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount2 * sizeof(GLfloat), &floatData[0]);
    glBufferSubData(GL_UNIFORM_BUFFER, blockSize2 + kVectorCount1 * sizeof(GLfloat),
                    kVectorCount1 * sizeof(GLfloat), &floatData[kFloatCount2 + kVectorCount1]);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        if (positionToTest[i][1] > 31 && positionToTest[i][1] < 64)
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::yellow);
        }
        else
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::cyan);
        }
    }
}

// Test a uniform block with large struct array member and a uniform block with small
// struct array member in the different programs, and they share a uniform buffer.
TEST_P(UniformBufferTest, UniformBlocksInDiffProgramShareUniformBuffer)
{
    GLint64 maxUniformBlockSize;
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    std::ostringstream stream1;
    std::ostringstream stream2;
    GLuint arraySize1, arraySize2;
    // Ensure that shader uniform block do not exceed MAX_UNIFORM_BLOCK_SIZE limit.
    if (maxUniformBlockSize >= 16384 && maxUniformBlockSize < 32768)
    {
        arraySize1 = 128;
        arraySize2 = 8;
        stream1 << "const uint arraySize1 = 128u;\n"
                   "const uint divisor1 = 128u;\n"
                   "const uint divisor2 = 32u;\n";
        stream2 << "const uint arraySize2 = 8u;\n"
                   "const uint divisor3 = 16u;\n";
    }
    else if (maxUniformBlockSize >= 32768 && maxUniformBlockSize < 65536)
    {
        arraySize1 = 256;
        arraySize2 = 16;
        stream1 << "const uint arraySize1 = 256u;\n"
                   "const uint divisor1 = 64u;\n"
                   "const uint divisor2 = 16u;\n";
        stream2 << "const uint arraySize2 = 16u;\n"
                   "const uint divisor3 = 8u;\n";
    }
    else
    {
        arraySize1 = 512;
        arraySize2 = 32;
        stream1 << "const uint arraySize1 = 512u;\n"
                   "const uint divisor1 = 32u;\n"
                   "const uint divisor2 = 8u;\n";
        stream2 << "const uint arraySize2 = 32u;\n"
                   "const uint divisor3 = 4u;\n";
    }

    const std::string &kFS1 =
        "#version 300 es\n"
        "precision highp float;\n" +
        stream1.str() +
        "out vec4 my_FragColor;\n"
        "struct S { mat4 color;};\n"
        "layout(std140) uniform buffer { S s[arraySize1]; };\n"
        "void main()\n"
        "{\n"
        "    uvec2 coord = uvec2(floor(gl_FragCoord.xy));\n"
        "    uint index = coord.x +  coord.y * 128u;\n"
        "    uint index_x = index / divisor1;\n"
        "    uint index_y = (index % divisor1) / divisor2;\n"
        "    my_FragColor = s[index_x].color[index_y];\n"
        "}\n";

    const std::string &kFS2 =
        "#version 300 es\n"
        "precision highp float;\n" +
        stream2.str() +
        "out vec4 my_FragColor;\n"
        "struct S { mat4 color;};\n"
        "layout(std140) uniform buffer { S s[arraySize2]; };\n"
        "void main()\n"
        "{\n"
        "    uvec2 coord = uvec2(floor(gl_FragCoord.xy));\n"
        "    uint index_x = coord.x / divisor3;\n"
        "    uint index_y = coord.x % 4u;\n"
        "    my_FragColor = s[index_x].color[index_y];\n"
        "}\n";

    GLint blockSize1, blockSize2;
    ANGLE_GL_PROGRAM(program1, essl3_shaders::vs::Simple(), kFS1.c_str());
    ANGLE_GL_PROGRAM(program2, essl3_shaders::vs::Simple(), kFS2.c_str());
    GLint uniformBufferIndex1 = glGetUniformBlockIndex(program1, "buffer");
    GLint uniformBufferIndex2 = glGetUniformBlockIndex(program2, "buffer");
    glGetActiveUniformBlockiv(program1, uniformBufferIndex1, GL_UNIFORM_BLOCK_DATA_SIZE,
                              &blockSize1);
    glGetActiveUniformBlockiv(program2, uniformBufferIndex2, GL_UNIFORM_BLOCK_DATA_SIZE,
                              &blockSize2);

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, std::max(blockSize1, blockSize2), nullptr, GL_STATIC_DRAW);

    glBindBufferRange(GL_UNIFORM_BUFFER, 0, mUniformBuffer, 0, blockSize2);
    glUniformBlockBinding(program2, uniformBufferIndex2, 0);
    glBindBufferRange(GL_UNIFORM_BUFFER, 1, mUniformBuffer, 0, blockSize1);
    glUniformBlockBinding(program1, uniformBufferIndex1, 1);

    const GLuint kVectorPerMat   = 4;
    const GLuint kFloatPerVector = 4;
    GLuint kVectorCount1         = arraySize1 * kVectorPerMat;
    GLuint kVectorCount2         = arraySize2 * kVectorPerMat;
    GLuint kFloatCount1          = kVectorCount1 * kFloatPerVector;
    GLuint kFloatCount2          = kVectorCount2 * kFloatPerVector;
    GLuint kFloatCount           = kFloatCount1;
    std::vector<GLfloat> floatData(kFloatCount, 0.0f);
    const GLuint kPositionCount                    = 12;
    unsigned int positionToTest[kPositionCount][2] = {{0, 0},  {75, 0},  {98, 13},  {31, 31},
                                                      {0, 32}, {65, 33}, {23, 54},  {63, 63},
                                                      {0, 64}, {43, 86}, {53, 100}, {127, 127}};

    for (GLuint i = 0; i < kVectorCount1; i++)
    {
        floatData[4 * i + 2] = 1.0f;
        floatData[4 * i + 3] = 1.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), &floatData[0]);
    drawQuad(program1.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::blue);
    }

    for (GLuint i = 0; i < kVectorCount2; i++)
    {
        floatData[4 * i + 1] = 1.0f;
        floatData[4 * i + 2] = 0.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount2 * sizeof(GLfloat), &floatData[0]);
    drawQuad(program2.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::green);
    }

    for (GLuint i = kVectorCount2; i < kVectorCount1 / 2; i++)
    {
        floatData[4 * i + 1] = 1.0f;
        floatData[4 * i + 2] = 0.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, kFloatCount2 * sizeof(GLfloat),
                    (kFloatCount1 / 2 - kFloatCount2) * sizeof(GLfloat), &floatData[0]);
    drawQuad(program1.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        if (positionToTest[i][1] < 64)
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::green);
        }
        else
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::blue);
        }
    }
}

// Test two uniform blocks share the same uniform buffer. On D3D backend, one uniform
// block with a large array member will be translated to StructuredBuffer, and the
// other uniform block will be translated to cbuffer, this case verifies that update
// buffer data correctly.
TEST_P(UniformBufferTest, UniformBlocksSharedSameUniformBuffer)
{
    ANGLE_SKIP_TEST_IF(IsIntel() && IsOSX());

    GLint64 maxUniformBlockSize;
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    std::ostringstream stream;
    GLuint arraySize;
    // Ensure that shader uniform block do not exceed MAX_UNIFORM_BLOCK_SIZE limit.
    if (maxUniformBlockSize >= 16384 && maxUniformBlockSize < 32768)
    {
        arraySize = 128;
        stream << "const uint arraySize = 128u;\n"
                  "const uint divisor1 = 128u;\n"
                  "const uint divisor2 = 32u;\n";
    }
    else if (maxUniformBlockSize >= 32768 && maxUniformBlockSize < 65536)
    {
        arraySize = 256;
        stream << "const uint arraySize = 256u;\n"
                  "const uint divisor1 = 64u;\n"
                  "const uint divisor2 = 16u;\n";
    }
    else
    {
        arraySize = 512;
        stream << "const uint arraySize = 512u;\n"
                  "const uint divisor1 = 32u;\n"
                  "const uint divisor2 = 8u;\n";
    }

    const std::string &kFS =
        "#version 300 es\n"
        "precision highp float;\n" +
        stream.str() +
        "out vec4 my_FragColor;\n"
        "struct S { mat4 color;};\n"
        "layout(std140) uniform buffer { S s[arraySize]; };\n"
        "layout(std140) uniform buffer1 { vec4 factor; };\n"
        "void main()\n"
        "{\n"
        "    uvec2 coord = uvec2(floor(gl_FragCoord.xy));\n"
        "    uint index = coord.x +  coord.y * 128u;\n"
        "    uint index_x = index / divisor1;\n"
        "    uint index_y = (index % divisor1) / divisor2;\n"
        "    my_FragColor = s[index_x].color[index_y] + factor;\n"
        "}\n";

    GLint blockSize;
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS.c_str());
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "buffer");
    glGetActiveUniformBlockiv(program, uniformBufferIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);
    GLint uniformBufferIndex1 = glGetUniformBlockIndex(program, "buffer1");
    GLint alignment;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
    while (alignment >= 0 && alignment < 16)
    {
        alignment += alignment;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, alignment + blockSize, nullptr, GL_STATIC_DRAW);

    const GLuint kVectorPerMat   = 4;
    const GLuint kFloatPerVector = 4;
    GLuint kVectorCount          = arraySize * kVectorPerMat;
    GLuint kFloatCount           = kVectorCount * kFloatPerVector;
    std::vector<GLfloat> floatData(kFloatCount, 0.0f);
    std::vector<GLfloat> floatData1(4, 0.0f);
    const GLuint kPositionCount                    = 12;
    unsigned int positionToTest[kPositionCount][2] = {{0, 0},  {75, 0},  {98, 13},  {31, 31},
                                                      {0, 32}, {65, 33}, {23, 54},  {63, 63},
                                                      {0, 64}, {43, 86}, {53, 100}, {127, 127}};

    for (GLuint i = 0; i < kVectorCount; i++)
    {
        floatData[4 * i + 2] = 0.5f;
        floatData[4 * i + 3] = 0.5f;
    }
    floatData1[0] = 1.0f;
    floatData1[1] = 0.0f;
    floatData1[2] = 0.5f;
    floatData1[3] = 0.5f;

    glBindBufferRange(GL_UNIFORM_BUFFER, 0, mUniformBuffer, 0, 4 * sizeof(float));
    glUniformBlockBinding(program, uniformBufferIndex1, 0);
    glBindBufferRange(GL_UNIFORM_BUFFER, 1, mUniformBuffer, alignment,
                      kVectorCount * kFloatPerVector * sizeof(float));
    glUniformBlockBinding(program, uniformBufferIndex, 1);

    glBufferSubData(GL_UNIFORM_BUFFER, alignment, kFloatCount * sizeof(GLfloat), floatData.data());
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 4 * sizeof(GLfloat), floatData1.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::magenta);
    }

    for (GLuint i = 0; i < kVectorCount; i++)
    {
        floatData[4 * i + 1] = 0.5f;
        floatData[4 * i + 2] = 0.0f;
    }
    floatData1[0] = 0.0f;
    floatData1[1] = 0.5f;
    floatData1[2] = 0.0f;
    floatData1[3] = 0.5f;
    glBufferSubData(GL_UNIFORM_BUFFER, alignment, kFloatCount * sizeof(GLfloat), floatData.data());
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 4 * sizeof(GLfloat), floatData1.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::green);
    }
}

// Compile uniform buffer with a large matrix array member.
TEST_P(UniformBufferTest, UniformBlockWithOneLargeMatrixArrayIndexAccess)
{
    GLint64 maxUniformBlockSize;
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    std::ostringstream stream;

    // Ensure that shader uniform block do not exceed MAX_UNIFORM_BLOCK_SIZE limit.
    if (maxUniformBlockSize < 16384)
    {
        return;
    }

    const char *kFS = R"(#version 300 es
precision mediump float;

uniform uint index;

struct S { uvec4 idx; };

layout(std140) uniform idxbuf { S idxArray[2]; };

layout(std140) uniform buffer1 { mat4 s1[128]; };
layout(std140) uniform buffer2 { mat4 s2[128]; } buf2[2];

out vec4 fragColor;
void main()
{
  fragColor = s1[1][0] + s1[index][1] + s1[idxArray[0].idx.x][idxArray[1].idx.z]
  + buf2[0].s2[1][0] + buf2[1].s2[index][1] + buf2[0].s2[idxArray[0].idx.y][idxArray[1].idx.z]
  + vec4(buf2[1].s2[index][1][index], s1[1][0][2], s1[idxArray[0].idx.x][idxArray[1].idx.z][2],
  buf2[0].s2[idxArray[0].idx.y][idxArray[1].idx.z][3]);
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    ASSERT_GL_NO_ERROR();
}

// Test uniform buffer with a large matrix array member.
TEST_P(UniformBufferTest, UniformBlockWithOneLargeMatrixArray)
{
    GLint64 maxUniformBlockSize;
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    std::ostringstream stream;
    GLuint arraySize;
    // Ensure that shader uniform block do not exceed MAX_UNIFORM_BLOCK_SIZE limit.
    if (maxUniformBlockSize >= 16384 && maxUniformBlockSize < 32768)
    {
        arraySize = 128;
        stream << "const uint arraySize = 128u;\n"
                  "const uint divisor1 = 128u;\n"
                  "const uint divisor2 = 32u;\n";
    }
    else if (maxUniformBlockSize >= 32768 && maxUniformBlockSize < 65536)
    {
        arraySize = 256;
        stream << "const uint arraySize = 256u;\n"
                  "const uint divisor1 = 64u;\n"
                  "const uint divisor2 = 16u;\n";
    }
    else
    {
        arraySize = 512;
        stream << "const uint arraySize = 512u;\n"
                  "const uint divisor1 = 32u;\n"
                  "const uint divisor2 = 8u;\n";
    }

    const std::string &kFS =
        "#version 300 es\n"
        "precision highp float;\n" +
        stream.str() +
        "out vec4 my_FragColor;\n"
        "layout(std140) uniform buffer { mat4 s[arraySize]; };\n"
        "void main()\n"
        "{\n"
        "    uvec2 coord = uvec2(floor(gl_FragCoord.xy));\n"
        "    uint index = coord.x +  coord.y * 128u;\n"
        "    uint index_x = index / divisor1;\n"
        "    uint index_y = (index % divisor1) / divisor2;\n"
        "    my_FragColor = s[index_x][index_y];\n"
        "}\n";

    GLint blockSize;
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS.c_str());
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "buffer");
    glGetActiveUniformBlockiv(program, uniformBufferIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, blockSize, nullptr, GL_STATIC_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);

    const GLuint kVectorPerMat   = 4;
    const GLuint kFloatPerVector = 4;
    GLuint kVectorCount          = arraySize * kVectorPerMat;
    GLuint kFloatCount           = kVectorCount * kFloatPerVector;
    std::vector<GLfloat> floatData(kFloatCount, 0.0f);
    const GLuint kPositionCount                    = 12;
    unsigned int positionToTest[kPositionCount][2] = {{0, 0},  {75, 0},  {98, 13},  {31, 31},
                                                      {0, 32}, {65, 33}, {23, 54},  {63, 63},
                                                      {0, 64}, {43, 86}, {53, 100}, {127, 127}};

    for (GLuint i = 0; i < kVectorCount; i++)
    {
        floatData[4 * i + 2] = 1.0f;
        floatData[4 * i + 3] = 1.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::blue);
    }

    for (GLuint i = 0; i < kVectorCount; i++)
    {
        floatData[4 * i + 1] = 1.0f;
        floatData[4 * i + 2] = 0.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::green);
    }

    for (GLuint i = kVectorCount / 4; i < kVectorCount / 2; i++)
    {
        floatData[4 * i]     = 1.0f;
        floatData[4 * i + 1] = 0.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        if (positionToTest[i][1] > 31 && positionToTest[i][1] < 64)
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::red);
        }
        else
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::green);
        }
    }
}

// Test uniform buffer with a large matrix array member, which using row major qualifier.
TEST_P(UniformBufferTest, UniformBlockWithOneLargeMatrixArrayWithRowMajorQualifier)
{
    // http://anglebug.com/3837 , http://anglebug.com/2273
    ANGLE_SKIP_TEST_IF(IsOSX() || IsAndroid() || (IsAMD() && IsOpenGL()) ||
                       (IsLinux() && IsIntel() && IsOpenGL()));
    GLint64 maxUniformBlockSize;
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    std::ostringstream stream;
    GLuint arraySize;
    // Ensure that shader uniform block do not exceed MAX_UNIFORM_BLOCK_SIZE limit.
    if (maxUniformBlockSize >= 16384 && maxUniformBlockSize < 32768)
    {
        arraySize = 128;
        stream << "const uint arraySize = 128u;\n"
                  "const uint divisor1 = 128u;\n"
                  "const uint divisor2 = 32u;\n";
    }
    else if (maxUniformBlockSize >= 32768 && maxUniformBlockSize < 65536)
    {
        arraySize = 256;
        stream << "const uint arraySize = 256u;\n"
                  "const uint divisor1 = 64u;\n"
                  "const uint divisor2 = 16u;\n";
    }
    else
    {
        arraySize = 512;
        stream << "const uint arraySize = 512u;\n"
                  "const uint divisor1 = 32u;\n"
                  "const uint divisor2 = 8u;\n";
    }

    const std::string &kFS =
        "#version 300 es\n"
        "precision highp float;\n" +
        stream.str() +
        "out vec4 my_FragColor;\n"
        "layout(std140, row_major) uniform buffer { mat4 s[arraySize]; };\n"
        "void main()\n"
        "{\n"
        "    uvec2 coord = uvec2(floor(gl_FragCoord.xy));\n"
        "    uint index = coord.x +  coord.y * 128u;\n"
        "    uint index_x = index / divisor1;\n"
        "    uint index_y = (index % divisor1) / divisor2;\n"
        "    my_FragColor = s[index_x][index_y];\n"
        "}\n";

    GLint blockSize;
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS.c_str());
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "buffer");
    glGetActiveUniformBlockiv(program, uniformBufferIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, blockSize, nullptr, GL_STATIC_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);

    const GLuint kVectorPerMat   = 4;
    const GLuint kFloatPerVector = 4;
    GLuint kVectorCount          = arraySize * kVectorPerMat;
    GLuint kFloatCount           = kVectorCount * kFloatPerVector;
    std::vector<GLfloat> floatData(kFloatCount, 0.0f);
    const GLuint kPositionCount                    = 12;
    unsigned int positionToTest[kPositionCount][2] = {{0, 0},  {75, 0},  {98, 13},  {31, 31},
                                                      {0, 32}, {65, 33}, {23, 54},  {63, 63},
                                                      {0, 64}, {43, 86}, {53, 100}, {127, 127}};

    for (GLuint i = 0; i < kVectorCount / 4; i++)
    {
        for (GLuint j = 0; j < 4; j++)
        {
            floatData[16 * i + 8 + j]  = 1.0f;
            floatData[16 * i + 12 + j] = 1.0f;
        }
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::blue);
    }

    for (GLuint i = 0; i < kVectorCount / 4; i++)
    {
        for (GLuint j = 0; j < 4; j++)
        {
            floatData[16 * i + 4 + j] = 1.0f;
            floatData[16 * i + 8 + j] = 0.0f;
        }
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::green);
    }

    for (GLuint i = kVectorCount / 16; i < kVectorCount / 8; i++)
    {
        for (GLuint j = 0; j < 4; j++)
        {
            floatData[16 * i + j]     = 1.0f;
            floatData[16 * i + 4 + j] = 0.0f;
        }
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        if (positionToTest[i][1] > 31 && positionToTest[i][1] < 64)
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::red);
        }
        else
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::green);
        }
    }
}

// Test instance array uniform buffer with a large matrix array member.
TEST_P(UniformBufferTest, InstanceArrayUniformBlockWithOneLargeMatrixArray)
{
    GLint64 maxUniformBlockSize;
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    std::ostringstream stream;
    GLuint arraySize;
    // Ensure that shader uniform block do not exceed MAX_UNIFORM_BLOCK_SIZE limit.
    if (maxUniformBlockSize >= 16384 && maxUniformBlockSize < 32768)
    {
        arraySize = 128;
        stream << "const uint arraySize = 128u;\n"
                  "const uint divisor1 = 128u;\n"
                  "const uint divisor2 = 32u;\n";
    }
    else if (maxUniformBlockSize >= 32768 && maxUniformBlockSize < 65536)
    {
        arraySize = 256;
        stream << "const uint arraySize = 256u;\n"
                  "const uint divisor1 = 64u;\n"
                  "const uint divisor2 = 16u;\n";
    }
    else
    {
        arraySize = 512;
        stream << "const uint arraySize = 512u;\n"
                  "const uint divisor1 = 32u;\n"
                  "const uint divisor2 = 8u;\n";
    }

    const std::string &kFS =
        "#version 300 es\n"
        "precision highp float;\n" +
        stream.str() +
        "out vec4 my_FragColor;\n"
        "layout(std140) uniform buffer { mat4 s[arraySize]; } instance[2];\n"
        "void main()\n"
        "{\n"
        "    uvec2 coord = uvec2(floor(gl_FragCoord.xy));\n"
        "    uint index = coord.x +  coord.y * 128u;\n"
        "    uint index_x = index / divisor1;\n"
        "    uint index_y = (index % divisor1) / divisor2;\n"
        "    my_FragColor = instance[0].s[index_x][index_y] + "
        "instance[1].s[index_x][index_y];\n"
        "}\n";

    GLint blockSize0, blockSize1;
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS.c_str());
    GLint uniformBufferIndex0 = glGetUniformBlockIndex(program, "buffer[0]");
    GLint uniformBufferIndex1 = glGetUniformBlockIndex(program, "buffer[1]");
    glGetActiveUniformBlockiv(program, uniformBufferIndex0, GL_UNIFORM_BLOCK_DATA_SIZE,
                              &blockSize0);
    glGetActiveUniformBlockiv(program, uniformBufferIndex1, GL_UNIFORM_BLOCK_DATA_SIZE,
                              &blockSize1);

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, blockSize0, nullptr, GL_STATIC_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex0, 0);

    const GLuint kVectorPerMat   = 4;
    const GLuint kFloatPerVector = 4;
    GLuint kVectorCount          = arraySize * kVectorPerMat;
    GLuint kFloatCount           = kVectorCount * kFloatPerVector;
    std::vector<GLfloat> floatData0(kFloatCount, 0.0f);
    std::vector<GLfloat> floatData1(kFloatCount, 0.0f);
    const GLuint kPositionCount                    = 12;
    unsigned int positionToTest[kPositionCount][2] = {{0, 0},  {75, 0},  {98, 13},  {31, 31},
                                                      {0, 32}, {65, 33}, {23, 54},  {63, 63},
                                                      {0, 64}, {43, 86}, {53, 100}, {127, 127}};

    for (GLuint i = 0; i < kVectorCount; i++)
    {
        floatData0[4 * i + 2] = 1.0f;
        floatData0[4 * i + 3] = 1.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData0.data());

    GLBuffer uniformBuffer1;
    glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffer1);
    glBufferData(GL_UNIFORM_BUFFER, blockSize1, nullptr, GL_STATIC_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, 1, uniformBuffer1);
    glUniformBlockBinding(program, uniformBufferIndex0, 1);

    for (GLuint i = 0; i < kVectorCount; i++)
    {
        floatData1[4 * i + 1] = 1.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData1.data());

    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::cyan);
    }

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    for (GLuint i = 0; i < kVectorCount; i++)
    {
        floatData0[4 * i]     = 1.0f;
        floatData0[4 * i + 2] = 0.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData0.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::yellow);
    }

    glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffer1);
    for (GLuint i = kVectorCount / 4; i < kVectorCount / 2; i++)
    {
        floatData1[4 * i + 2] = 1.0f;
        floatData1[4 * i + 1] = 0.0f;
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kFloatCount * sizeof(GLfloat), floatData1.data());
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    for (GLuint i = 0; i < kPositionCount; i++)
    {
        if (positionToTest[i][1] > 31 && positionToTest[i][1] < 64)
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::magenta);
        }
        else
        {
            EXPECT_PIXEL_COLOR_EQ(positionToTest[i][0], positionToTest[i][1], GLColor::yellow);
        }
    }
}

// Test a uniform block where an array of row-major matrices is dynamically indexed.
TEST_P(UniformBufferTest, Std140UniformBlockWithDynamicallyIndexedRowMajorArray)
{
    // http://anglebug.com/3837 , http://anglebug.com/2273
    ANGLE_SKIP_TEST_IF((IsLinux() && IsIntel() && IsOpenGL()) || IsOSX());

    constexpr char kFS[] =
        R"(#version 300 es

        precision highp float;
        out vec4 my_FragColor;

        uniform int u_zero;

        layout(std140, row_major) uniform matrixBuffer {
            mat4 u_mats[1];
        };

        void main() {
            float f = u_mats[u_zero + 0][2][1];
            my_FragColor = vec4(1.0 - f, f, 0.0, 1.0);
        })";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    GLint uniformBufferIndex = glGetUniformBlockIndex(program, "matrixBuffer");

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    const GLsizei kElementsPerMatrix = 16;  // Each mat2 row gets padded into a vec4.
    const GLsizei kBytesPerElement   = 4;
    const GLsizei kDataSize          = kElementsPerMatrix * kBytesPerElement;
    std::vector<GLubyte> v(kDataSize, 0);
    float *vAsFloat = reinterpret_cast<float *>(v.data());
    // Write out this initializer to make it clearer what the matrix contains.
    float matrixData[kElementsPerMatrix] = {
        // clang-format off
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        // clang-format on
    };
    for (int ii = 0; ii < kElementsPerMatrix; ++ii)
    {
        vAsFloat[ii] = matrixData[ii];
    }
    glBufferData(GL_UNIFORM_BUFFER, kDataSize, v.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);
    glUniformBlockBinding(program, uniformBufferIndex, 0);
    GLint indexLoc = glGetUniformLocation(program, "u_zero");
    glUseProgram(program);
    glUniform1i(indexLoc, 0);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(0, 255, 0, 255), 5);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES3(UniformBufferTest);
ANGLE_INSTANTIATE_TEST_ES31(UniformBufferTest31);

}  // namespace
