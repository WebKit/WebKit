//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SimpleOperationTest:
//   Basic GL commands such as linking a program, initializing a buffer, etc.

#include "test_utils/ANGLETest.h"

#include <vector>

#include "random_utils.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class SimpleOperationTest : public ANGLETest
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
};

void SimpleOperationTest::verifyBuffer(const std::vector<uint8_t> &data, GLenum binding)
{
    if (!extensionEnabled("GL_EXT_map_buffer_range"))
    {
        return;
    }

    uint8_t *mapPointer =
        static_cast<uint8_t *>(glMapBufferRangeEXT(GL_ARRAY_BUFFER, 0, 1024, GL_MAP_READ_BIT));
    ASSERT_GL_NO_ERROR();

    std::vector<uint8_t> readbackData(data.size());
    memcpy(readbackData.data(), mapPointer, data.size());
    glUnmapBufferOES(GL_ARRAY_BUFFER);

    EXPECT_EQ(data, readbackData);
}

TEST_P(SimpleOperationTest, CompileVertexShader)
{
    const std::string source =
        R"(attribute vec4 a_input;
        void main()
        {
            gl_Position = a_input;
        })";

    GLuint shader = CompileShader(GL_VERTEX_SHADER, source);
    EXPECT_NE(shader, 0u);
    glDeleteShader(shader);

    EXPECT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, CompileFragmentShader)
{
    const std::string source =
        R"(precision mediump float;
        varying vec4 v_input;
        void main()
        {
            gl_FragColor = v_input;
        })";

    GLuint shader = CompileShader(GL_FRAGMENT_SHADER, source);
    EXPECT_NE(shader, 0u);
    glDeleteShader(shader);

    EXPECT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, LinkProgram)
{
    const std::string vsSource =
        R"(void main()
        {
            gl_Position = vec4(1.0, 1.0, 1.0, 1.0);
        })";

    const std::string fsSource =
        R"(void main()
        {
            gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        })";

    GLuint program = CompileProgram(vsSource, fsSource);
    EXPECT_NE(program, 0u);
    glDeleteProgram(program);

    EXPECT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, LinkProgramWithUniforms)
{
    const std::string vsSource =
        R"(void main()
        {
            gl_Position = vec4(1.0, 1.0, 1.0, 1.0);
        })";

    const std::string fsSource =
        R"(precision mediump float;
        uniform vec4 u_input;
        void main()
        {
            gl_FragColor = u_input;
        })";

    GLuint program = CompileProgram(vsSource, fsSource);
    EXPECT_NE(program, 0u);

    GLint uniformLoc = glGetUniformLocation(program, "u_input");
    EXPECT_NE(-1, uniformLoc);

    glDeleteProgram(program);

    EXPECT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, LinkProgramWithAttributes)
{
    const std::string vsSource =
        R"(attribute vec4 a_input;
        void main()
        {
            gl_Position = a_input;
        })";

    const std::string fsSource =
        R"(void main()
        {
            gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        })";

    GLuint program = CompileProgram(vsSource, fsSource);
    EXPECT_NE(program, 0u);

    GLint attribLoc = glGetAttribLocation(program, "a_input");
    EXPECT_NE(-1, attribLoc);

    glDeleteProgram(program);

    EXPECT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, BufferDataWithData)
{
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());

    std::vector<uint8_t> data(1024);
    FillVectorWithRandomUBytes(&data);
    glBufferData(GL_ARRAY_BUFFER, data.size(), &data[0], GL_STATIC_DRAW);

    verifyBuffer(data, GL_ARRAY_BUFFER);

    EXPECT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, BufferDataWithNoData)
{
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());
    glBufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW);

    EXPECT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, BufferSubData)
{
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());

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

    EXPECT_GL_NO_ERROR();
}

// Simple quad test.
TEST_P(SimpleOperationTest, DrawQuad)
{
    const std::string &vertexShader =
        "attribute vec3 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 1);\n"
        "}";
    const std::string &fragmentShader =
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(0, 1, 0, 1);\n"
        "}";
    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);

    drawQuad(program.get(), "position", 0.5f, 1.0f, true);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Simple repeated draw and swap test.
TEST_P(SimpleOperationTest, DrawQuadAndSwap)
{
    const std::string &vertexShader =
        "attribute vec3 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 1);\n"
        "}";
    const std::string &fragmentShader =
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(0, 1, 0, 1);\n"
        "}";
    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);

    for (int i = 0; i < 8; ++i)
    {
        drawQuad(program.get(), "position", 0.5f, 1.0f, true);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
        swapBuffers();
    }

    EXPECT_GL_NO_ERROR();
}

// Simple indexed quad test.
TEST_P(SimpleOperationTest, DrawIndexedQuad)
{
    const std::string vertexShader =
        "attribute vec3 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 1);\n"
        "}";
    const std::string fragmentShader =
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(0, 1, 0, 1);\n"
        "}";
    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);

    drawIndexedQuad(program.get(), "position", 0.5f, 1.0f, true);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Draw with a fragment uniform.
TEST_P(SimpleOperationTest, DrawQuadWithFragmentUniform)
{
    const std::string &vertexShader =
        "attribute vec3 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 1);\n"
        "}";
    const std::string &fragmentShader =
        "uniform mediump vec4 color;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = color;\n"
        "}";
    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);

    GLint location = glGetUniformLocation(program, "color");
    ASSERT_NE(-1, location);

    glUseProgram(program);
    glUniform4f(location, 0.0f, 1.0f, 0.0f, 1.0f);

    drawQuad(program.get(), "position", 0.5f, 1.0f, true);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Draw with a vertex uniform.
TEST_P(SimpleOperationTest, DrawQuadWithVertexUniform)
{
    const std::string &vertexShader =
        "attribute vec3 position;\n"
        "uniform vec4 color;\n"
        "varying vec4 vcolor;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 1);\n"
        "    vcolor = color;\n"
        "}";
    const std::string &fragmentShader =
        "varying mediump vec4 vcolor;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vcolor;\n"
        "}";
    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);

    GLint location = glGetUniformLocation(program, "color");
    ASSERT_NE(-1, location);

    glUseProgram(program);
    glUniform4f(location, 0.0f, 1.0f, 0.0f, 1.0f);

    drawQuad(program.get(), "position", 0.5f, 1.0f, true);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Draw with two uniforms.
TEST_P(SimpleOperationTest, DrawQuadWithTwoUniforms)
{
    const std::string &vertexShader =
        "attribute vec3 position;\n"
        "uniform vec4 color1;\n"
        "varying vec4 vcolor1;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 1);\n"
        "    vcolor1 = color1;\n"
        "}";
    const std::string &fragmentShader =
        "uniform mediump vec4 color2;\n"
        "varying mediump vec4 vcolor1;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vcolor1 + color2;\n"
        "}";
    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);

    GLint location1 = glGetUniformLocation(program, "color1");
    ASSERT_NE(-1, location1);

    GLint location2 = glGetUniformLocation(program, "color2");
    ASSERT_NE(-1, location2);

    glUseProgram(program);
    glUniform4f(location1, 0.0f, 1.0f, 0.0f, 1.0f);
    glUniform4f(location2, 1.0f, 0.0f, 0.0f, 1.0f);

    drawQuad(program.get(), "position", 0.5f, 1.0f, true);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::yellow);
}

// Tests a shader program with more than one vertex attribute, with vertex buffers.
TEST_P(SimpleOperationTest, ThreeVertexAttributes)
{
    const std::string vertexShader =
        R"(attribute vec2 position;
attribute vec4 color1;
attribute vec4 color2;
varying vec4 color;
void main()
{
    gl_Position = vec4(position, 0, 1);
    color = color1 + color2;
})";

    const std::string fragmentShader =
        R"(precision mediump float;
varying vec4 color;
void main()
{
    gl_FragColor = color;
}
)";

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);

    glUseProgram(program);

    GLint color1Loc = glGetAttribLocation(program, "color1");
    GLint color2Loc = glGetAttribLocation(program, "color2");
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

// Creates a texture, no other operations.
TEST_P(SimpleOperationTest, CreateTexture2DNoData)
{
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();
}

// Creates a texture, no other operations.
TEST_P(SimpleOperationTest, CreateTexture2DWithData)
{
    std::vector<GLColor> colors(16 * 16, GLColor::red);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors.data());
    ASSERT_GL_NO_ERROR();
}

// Creates a program with a texture.
TEST_P(SimpleOperationTest, LinkProgramWithTexture)
{
    ASSERT_NE(0u, get2DTexturedQuadProgram());
    EXPECT_GL_NO_ERROR();
}

// Creates a program with a texture and renders with it.
TEST_P(SimpleOperationTest, DrawWithTexture)
{
    std::array<GLColor, 4> colors = {
        {GLColor::red, GLColor::green, GLColor::blue, GLColor::yellow}};

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    draw2DTexturedQuad(0.5f, 1.0f, true);
    EXPECT_GL_NO_ERROR();

    int w = getWindowWidth() - 2;
    int h = getWindowHeight() - 2;

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(w, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, h, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(w, h, GLColor::yellow);
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
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glViewport(0, 0, kSize, kSize);

    const std::string &vertexShader =
        "attribute vec3 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 1);\n"
        "}";
    const std::string &fragmentShader =
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(0, 1, 0, 1);\n"
        "}";
    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
    drawQuad(program, "position", 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
ANGLE_INSTANTIATE_TEST(SimpleOperationTest,
                       ES2_D3D9(),
                       ES2_D3D11(EGL_EXPERIMENTAL_PRESENT_PATH_COPY_ANGLE),
                       ES2_D3D11(EGL_EXPERIMENTAL_PRESENT_PATH_FAST_ANGLE),
                       ES3_D3D11(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES(),
                       ES2_VULKAN());

} // namespace
