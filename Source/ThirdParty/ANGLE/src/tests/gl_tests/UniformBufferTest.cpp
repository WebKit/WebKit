//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

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

    virtual void SetUp()
    {
        ANGLETest::SetUp();

        const std::string vertexShaderSource = SHADER_SOURCE
        (   #version 300 es\n
            in vec4 position;
            void main()
            {
                gl_Position = position;
            }
        );
        const std::string fragmentShaderSource = SHADER_SOURCE
        (   #version 300 es\n
            precision highp float;
            uniform uni {
                vec4 color;
            };

            out vec4 fragColor;

            void main()
            {
                fragColor = color;
            }
        );

        mProgram = CompileProgram(vertexShaderSource, fragmentShaderSource);
        ASSERT_NE(mProgram, 0u);

        mUniformBufferIndex = glGetUniformBlockIndex(mProgram, "uni");
        ASSERT_NE(mUniformBufferIndex, -1);

        glGenBuffers(1, &mUniformBuffer);

        ASSERT_GL_NO_ERROR();
    }

    virtual void TearDown()
    {
        glDeleteBuffers(1, &mUniformBuffer);
        glDeleteProgram(mProgram);
        ANGLETest::TearDown();
    }

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
    drawQuad(mProgram, "position", 0.5f);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(0, 0, 128, 191, 64, 255, 1);
}

// Test that using a UBO with a non-zero offset and size actually works.
// The first step of this test renders a color from a UBO with a zero offset.
// The second step renders a color from a UBO with a non-zero offset.
TEST_P(UniformBufferTest, UniformBufferRange)
{
    // TODO(jmadill): Figure out why this fails on Intel.
    if (IsIntel() && IsD3D11())
    {
        std::cout << "Test skipped on Intel D3D11." << std::endl;
        return;
    }

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
    GLuint stride = 0;
    do
    {
        stride += alignment;
    }
    while (stride < vec4Size);

    std::vector<char> v(2 * stride);
    float *first = reinterpret_cast<float*>(v.data());
    float *second = reinterpret_cast<float*>(v.data() + stride);

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
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(px, py, 10, 20, 30, 40);

    // Bind the second part of the uniform buffer and draw
    // Furthermore the D3D11.1 backend will internally round the vec4Size (16 bytes) to a stride (256 bytes)
    // hence it will try to map the range [stride, 2 * stride] which is
    // out-of-bound of the buffer bufferSize = stride + vec4Size < 2 * stride.
    // Ensure that this behaviour works.
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, mUniformBuffer, stride, vec4Size);
    drawQuad(mProgram, "position", 0.5f);
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
    float *first = reinterpret_cast<float*>(v.data());

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
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(px, py, 10, 20, 30, 40);

    // Clear the framebuffer
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(px, py, 0, 0, 0, 0);

    // Try to bind the buffer to another binding point
    glUniformBlockBinding(mProgram, mUniformBufferIndex, 5);
    glBindBufferBase(GL_UNIFORM_BUFFER, 5, mUniformBuffer);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(px, py, 10, 20, 30, 40);
}

// Test that ANGLE handles used but unbound UBO.
// TODO: A test case shouldn't depend on the error code of an undefined behaviour. Move this to unit tests of the validation layer.
TEST_P(UniformBufferTest, UnboundUniformBuffer)
{
    glUniformBlockBinding(mProgram, mUniformBufferIndex, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Update a UBO many time and verify that ANGLE uses the latest version of the data.
// https://code.google.com/p/angleproject/issues/detail?id=965
TEST_P(UniformBufferTest, UniformBufferManyUpdates)
{
    // TODO(jmadill): Figure out why this fails on Intel OpenGL.
    if (IsIntel() && IsOpenGL())
    {
        std::cout << "Test skipped on Intel OpenGL." << std::endl;
        return;
    }

    int px = getWindowWidth() / 2;
    int py = getWindowHeight() / 2;

    ASSERT_GL_NO_ERROR();

    float data[4];

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(data), NULL, GL_DYNAMIC_DRAW);
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

        drawQuad(mProgram, "position", 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_EQ(px, py, i + 10, i + 20, i + 30, i + 40);
    }
}

// Use a large number of buffer ranges (compared to the actual size of the UBO)
TEST_P(UniformBufferTest, ManyUniformBufferRange)
{
    // TODO(jmadill): Figure out why this fails on Intel.
    if (IsIntel() && IsD3D11())
    {
        std::cout << "Test skipped on Intel D3D11." << std::endl;
        return;
    }
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
    GLuint stride = 0;
    do
    {
        stride += alignment;
    }
    while (stride < vec4Size);

    std::vector<char> v(8 * stride);

    for (size_t i = 0; i < 8; ++i)
    {
        float *data = reinterpret_cast<float*>(v.data() + i * stride);

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
        drawQuad(mProgram, "position", 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_EQ(px, py, 10 + i, 20 + i, 30 + i, 40 + i);
    }

    // Try to bind larger range
    for (size_t i = 0; i < 7; ++i)
    {
        glBindBufferRange(GL_UNIFORM_BUFFER, 0, mUniformBuffer, i * stride, 2 * stride);
        drawQuad(mProgram, "position", 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_EQ(px, py, 10 + i, 20 + i, 30 + i, 40 + i);
    }

    // Try to bind even larger range
    for (size_t i = 0; i < 5; ++i)
    {
        glBindBufferRange(GL_UNIFORM_BUFFER, 0, mUniformBuffer, i * stride, 4 * stride);
        drawQuad(mProgram, "position", 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_EQ(px, py, 10 + i, 20 + i, 30 + i, 40 + i);
    }
}

// Tests that active uniforms have the right names.
TEST_P(UniformBufferTest, ActiveUniformNames)
{
    const std::string &vertexShaderSource =
        "#version 300 es\n"
        "in vec2 position;\n"
        "out float v;\n"
        "uniform blockName {\n"
        "  float f;\n"
        "} instanceName;\n"
        "void main() {\n"
        "  v = instanceName.f;\n"
        "  gl_Position = vec4(position, 0, 1);\n"
        "}";

    const std::string &fragmentShaderSource =
        "#version 300 es\n"
        "precision highp float;\n"
        "in float v;\n"
        "out vec4 color;\n"
        "void main() {\n"
        "  color = vec4(v, 0, 0, 1);\n"
        "}";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    ASSERT_NE(0u, program);

    GLint activeUniforms;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &activeUniforms);

    ASSERT_EQ(1, activeUniforms);

    GLint maxLength, size;
    GLenum type;
    GLsizei length;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);
    std::vector<GLchar> strBuffer(maxLength + 1, 0);
    glGetActiveUniform(program, 0, maxLength, &length, &size, &type, &strBuffer[0]);

    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_GLENUM_EQ(GL_FLOAT, type);
    EXPECT_EQ("blockName.f", std::string(&strBuffer[0]));
}

// Tests active uniforms and blocks when the layout is std140, shared and packed.
TEST_P(UniformBufferTest, ActiveUniformNumberAndName)
{
    // TODO(Jiajia): Figure out why this fails on Intel on Mac.
    // This case can pass on Intel Mac-10.11/10.12. But it fails on Intel Mac-10.10.
    if (IsIntel() && IsOSX())
    {
        std::cout << "Test skipped on Intel on Mac." << std::endl;
        return;
    }

    // This case fails on all AMD platforms (Mac, Linux, Win).
    if (IsAMD())
    {
        std::cout << "Test skipped on AMD." << std::endl;
        return;
    }

    const std::string &vertexShaderSource =
        "#version 300 es\n"
        "in vec2 position;\n"
        "out float v;\n"
        "struct S {\n"
        "  highp ivec3 a;\n"
        "  mediump ivec2 b[4];\n"
        "};\n"
        "struct T {\n"
        "  S c[2];\n"
        "};\n"
        "layout(std140) uniform blockName0 {\n"
        "  S s0;\n"
        "  lowp vec2 v0;\n"
        "  T t0[2];\n"
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

    const std::string &fragmentShaderSource =
        "#version 300 es\n"
        "precision highp float;\n"
        "in float v;\n"
        "out vec4 color;\n"
        "void main() {\n"
        "  color = vec4(v, 0, 0, 1);\n"
        "}";

    ANGLE_GL_PROGRAM(program, vertexShaderSource, fragmentShaderSource);

    GLint activeUniforms;
    glGetProgramiv(program.get(), GL_ACTIVE_UNIFORMS, &activeUniforms);

    ASSERT_EQ(15, activeUniforms);

    GLint activeUniformBlocks;
    glGetProgramiv(program.get(), GL_ACTIVE_UNIFORM_BLOCKS, &activeUniformBlocks);

    ASSERT_EQ(3, activeUniformBlocks);

    GLint maxLength, size;
    GLenum type;
    GLsizei length;
    glGetProgramiv(program.get(), GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);
    std::vector<GLchar> strBuffer(maxLength + 1, 0);

    glGetActiveUniform(program.get(), 0, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("s0.a", std::string(&strBuffer[0]));

    glGetActiveUniform(program.get(), 1, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(4, size);
    EXPECT_EQ("s0.b[0]", std::string(&strBuffer[0]));

    glGetActiveUniform(program.get(), 2, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("v0", std::string(&strBuffer[0]));

    glGetActiveUniform(program.get(), 3, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("t0[0].c[0].a", std::string(&strBuffer[0]));

    glGetActiveUniform(program.get(), 4, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(4, size);
    EXPECT_EQ("t0[0].c[0].b[0]", std::string(&strBuffer[0]));

    glGetActiveUniform(program.get(), 5, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("t0[0].c[1].a", std::string(&strBuffer[0]));

    glGetActiveUniform(program.get(), 6, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(4, size);
    EXPECT_EQ("t0[0].c[1].b[0]", std::string(&strBuffer[0]));

    glGetActiveUniform(program.get(), 7, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("t0[1].c[0].a", std::string(&strBuffer[0]));

    glGetActiveUniform(program.get(), 8, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(4, size);
    EXPECT_EQ("t0[1].c[0].b[0]", std::string(&strBuffer[0]));

    glGetActiveUniform(program.get(), 9, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("t0[1].c[1].a", std::string(&strBuffer[0]));

    glGetActiveUniform(program.get(), 10, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(4, size);
    EXPECT_EQ("t0[1].c[1].b[0]", std::string(&strBuffer[0]));

    glGetActiveUniform(program.get(), 11, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("u0", std::string(&strBuffer[0]));

    glGetActiveUniform(program.get(), 12, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("blockName1.f1", std::string(&strBuffer[0]));

    glGetActiveUniform(program.get(), 13, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("blockName1.b1", std::string(&strBuffer[0]));

    glGetActiveUniform(program.get(), 14, maxLength, &length, &size, &type, &strBuffer[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, size);
    EXPECT_EQ("f2", std::string(&strBuffer[0]));
}

// Test that using a very large buffer to back a small uniform block works OK.
TEST_P(UniformBufferTest, VeryLarge)
{
    // TODO(jmadill): Figure out why this fails on Intel.
    // See http://crbug.com/593024
    if (IsIntel() && IsD3D11())
    {
        std::cout << "Test skipped on Intel D3D11." << std::endl;
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT);
    float floatData[4] = {0.5f, 0.75f, 0.25f, 1.0f};

    GLsizei bigSize = 4096 * 64;

    glBindBuffer(GL_UNIFORM_BUFFER, mUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, bigSize, nullptr, GL_STATIC_DRAW);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(float) * 4, floatData);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUniformBuffer);

    glUniformBlockBinding(mProgram, mUniformBufferIndex, 0);
    drawQuad(mProgram, "position", 0.5f);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(0, 0, 128, 191, 64, 255, 1);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
ANGLE_INSTANTIATE_TEST(UniformBufferTest,
                       ES3_D3D11(),
                       ES3_D3D11_FL11_1(),
                       ES3_D3D11_FL11_1_REFERENCE(),
                       ES3_OPENGL(),
                       ES3_OPENGLES());

} // namespace
