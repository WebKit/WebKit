//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DrawElementsTest:
//   Tests for indexed draws.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class DrawElementsTest : public ANGLETest
{
  protected:
    DrawElementsTest() : mProgram(0u)
    {
        setWindowWidth(64);
        setWindowHeight(64);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    ~DrawElementsTest()
    {
        for (GLuint indexBuffer : mIndexBuffers)
        {
            if (indexBuffer != 0)
            {
                glDeleteBuffers(1, &indexBuffer);
            }
        }

        for (GLuint vertexArray : mVertexArrays)
        {
            if (vertexArray != 0)
            {
                glDeleteVertexArrays(1, &vertexArray);
            }
        }

        for (GLuint vertexBuffer : mVertexBuffers)
        {
            if (vertexBuffer != 0)
            {
                glDeleteBuffers(1, &vertexBuffer);
            }
        }

        if (mProgram != 0u)
        {
            glDeleteProgram(mProgram);
        }
    }

    std::vector<GLuint> mIndexBuffers;
    std::vector<GLuint> mVertexArrays;
    std::vector<GLuint> mVertexBuffers;
    GLuint mProgram;
};

class WebGLDrawElementsTest : public DrawElementsTest
{
  public:
    WebGLDrawElementsTest() { setWebGLCompatibilityEnabled(true); }
};

// Test no error is generated when using client-side arrays, indices = nullptr and count = 0
TEST_P(DrawElementsTest, ClientSideNullptrArrayZeroCount)
{
    constexpr char kVS[] =
        "attribute vec3 a_pos;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_pos, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, essl1_shaders::fs::Blue());

    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);
    glUseProgram(program.get());

    const auto &vertices = GetQuadVertices();

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer.get());
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);
    ASSERT_GL_NO_ERROR();

    // "If drawElements is called with a count greater than zero, and no WebGLBuffer is bound to the
    // ELEMENT_ARRAY_BUFFER binding point, an INVALID_OPERATION error is generated."
    glDrawElements(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // count == 0 so it's fine to have no element array buffer bound.
    glDrawElements(GL_TRIANGLES, 0, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();
}

// Test a state desync that can occur when using a streaming index buffer in GL in concert with
// deleting the applied index buffer.
TEST_P(DrawElementsTest, DeletingAfterStreamingIndexes)
{
    // Init program
    constexpr char kVS[] =
        "attribute vec2 position;\n"
        "attribute vec2 testFlag;\n"
        "varying vec2 v_data;\n"
        "void main() {\n"
        "  gl_Position = vec4(position, 0, 1);\n"
        "  v_data = testFlag;\n"
        "}";

    constexpr char kFS[] =
        "varying highp vec2 v_data;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(v_data, 0, 1);\n"
        "}";

    mProgram = CompileProgram(kVS, kFS);
    ASSERT_NE(0u, mProgram);
    glUseProgram(mProgram);

    GLint positionLocation = glGetAttribLocation(mProgram, "position");
    ASSERT_NE(-1, positionLocation);

    GLint testFlagLocation = glGetAttribLocation(mProgram, "testFlag");
    ASSERT_NE(-1, testFlagLocation);

    mIndexBuffers.resize(3u);
    glGenBuffers(3, &mIndexBuffers[0]);

    mVertexArrays.resize(2);
    glGenVertexArrays(2, &mVertexArrays[0]);

    mVertexBuffers.resize(2);
    glGenBuffers(2, &mVertexBuffers[0]);

    std::vector<GLuint> indexData[2];
    indexData[0].push_back(0);
    indexData[0].push_back(1);
    indexData[0].push_back(2);
    indexData[0].push_back(2);
    indexData[0].push_back(3);
    indexData[0].push_back(0);

    indexData[1] = indexData[0];
    for (GLuint &item : indexData[1])
    {
        item += 4u;
    }

    std::vector<GLfloat> positionData = {// quad verts
                                         -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
                                         // Repeat position data
                                         -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

    std::vector<GLfloat> testFlagData = {// red
                                         1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                                         // green
                                         0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffers[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * indexData[0].size(), &indexData[0][0],
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffers[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * indexData[0].size(), &indexData[0][0],
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * indexData[1].size(), &indexData[1][0],
                 GL_STATIC_DRAW);

    // Initialize first vertex array with second index buffer
    glBindVertexArray(mVertexArrays[0]);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffers[1]);
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffers[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * positionData.size(), &positionData[0],
                 GL_STATIC_DRAW);
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, nullptr);
    glEnableVertexAttribArray(positionLocation);

    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffers[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * testFlagData.size(), &testFlagData[0],
                 GL_STATIC_DRAW);
    glVertexAttribPointer(testFlagLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, nullptr);
    glEnableVertexAttribArray(testFlagLocation);

    // Initialize second vertex array with first index buffer
    glBindVertexArray(mVertexArrays[1]);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffers[0]);

    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffers[0]);
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, nullptr);
    glEnableVertexAttribArray(positionLocation);

    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffers[1]);
    glVertexAttribPointer(testFlagLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, nullptr);
    glEnableVertexAttribArray(testFlagLocation);

    ASSERT_GL_NO_ERROR();

    glBindVertexArray(mVertexArrays[0]);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    glBindVertexArray(mVertexArrays[1]);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    glBindVertexArray(mVertexArrays[0]);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Trigger the bug here.
    glDeleteBuffers(1, &mIndexBuffers[2]);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    ASSERT_GL_NO_ERROR();
}

// Test drawing to part of the indices in an index buffer, and then all of them.
TEST_P(DrawElementsTest, PartOfIndexBufferThenAll)
{
    // Init program
    constexpr char kVS[] =
        "attribute vec2 position;\n"
        "attribute vec2 testFlag;\n"
        "varying vec2 v_data;\n"
        "void main() {\n"
        "  gl_Position = vec4(position, 0, 1);\n"
        "  v_data = testFlag;\n"
        "}";

    constexpr char kFS[] =
        "varying highp vec2 v_data;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(v_data, 0, 1);\n"
        "}";

    mProgram = CompileProgram(kVS, kFS);
    ASSERT_NE(0u, mProgram);
    glUseProgram(mProgram);

    GLint positionLocation = glGetAttribLocation(mProgram, "position");
    ASSERT_NE(-1, positionLocation);

    GLint testFlagLocation = glGetAttribLocation(mProgram, "testFlag");
    ASSERT_NE(-1, testFlagLocation);

    mIndexBuffers.resize(1);
    glGenBuffers(1, &mIndexBuffers[0]);

    mVertexArrays.resize(1);
    glGenVertexArrays(1, &mVertexArrays[0]);

    mVertexBuffers.resize(2);
    glGenBuffers(2, &mVertexBuffers[0]);

    std::vector<GLubyte> indexData[2];
    indexData[0].push_back(0);
    indexData[0].push_back(1);
    indexData[0].push_back(2);
    indexData[0].push_back(2);
    indexData[0].push_back(3);
    indexData[0].push_back(0);
    indexData[0].push_back(4);
    indexData[0].push_back(5);
    indexData[0].push_back(6);
    indexData[0].push_back(6);
    indexData[0].push_back(7);
    indexData[0].push_back(4);

    // Make a copy:
    indexData[1] = indexData[0];

    std::vector<GLfloat> positionData = {// quad verts
                                         -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
                                         // Repeat position data
                                         -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

    std::vector<GLfloat> testFlagData = {// red
                                         1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                                         // green
                                         0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffers[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLubyte) * indexData[0].size(), &indexData[0][0],
                 GL_STATIC_DRAW);

    glBindVertexArray(mVertexArrays[0]);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffers[0]);
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffers[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * positionData.size(), &positionData[0],
                 GL_STATIC_DRAW);
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, nullptr);
    glEnableVertexAttribArray(positionLocation);

    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffers[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * testFlagData.size(), &testFlagData[0],
                 GL_STATIC_DRAW);
    glVertexAttribPointer(testFlagLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, nullptr);
    glEnableVertexAttribArray(testFlagLocation);

    ASSERT_GL_NO_ERROR();

    // Draw with just the second set of 6 items, then first 6, and then the entire index buffer
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, reinterpret_cast<const void *>(6));
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Reload the buffer again with a copy of the same data
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLubyte) * indexData[1].size(), &indexData[1][0],
                 GL_STATIC_DRAW);

    // Draw with just the first 6 indices, and then with the entire index buffer
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Reload the buffer again with a copy of the same data
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLubyte) * indexData[0].size(), &indexData[0][0],
                 GL_STATIC_DRAW);

    // This time, do not check color between draws (which causes a flush):
    // Draw with just the second set of 6 items, then first 6, and then the entire index buffer
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, reinterpret_cast<const void *>(6));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr);
    glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    ASSERT_GL_NO_ERROR();
}

// Test that the offset in the index buffer is forced to be a multiple of the element size
TEST_P(WebGLDrawElementsTest, DrawElementsTypeAlignment)
{
    constexpr char kVS[] =
        "attribute vec3 a_pos;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_pos, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, essl1_shaders::fs::Blue());

    GLint posLocation = glGetAttribLocation(program, "a_pos");
    ASSERT_NE(-1, posLocation);
    glUseProgram(program);

    const auto &vertices = GetQuadVertices();

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);

    GLBuffer indexBuffer;
    const GLubyte indices1[] = {0, 0, 0, 0, 0, 0};
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices1), indices1, GL_STATIC_DRAW);

    ASSERT_GL_NO_ERROR();

    const char *zeroIndices = nullptr;

    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, zeroIndices);
    ASSERT_GL_NO_ERROR();

    const GLushort indices2[] = {0, 0, 0, 0, 0, 0};
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices2), indices2, GL_STATIC_DRAW);

    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, zeroIndices + 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

ANGLE_INSTANTIATE_TEST_ES3(DrawElementsTest);
ANGLE_INSTANTIATE_TEST_ES2(WebGLDrawElementsTest);
}  // namespace
