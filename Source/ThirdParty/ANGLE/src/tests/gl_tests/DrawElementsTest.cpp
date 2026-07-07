//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DrawElementsTest:
//   Tests for indexed draws.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class DrawElementsTest : public ANGLETest<>
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

template <typename T, typename U>
std::vector<T> convertIndexBufferWithRestarts(const std::vector<U> &input)
{
    constexpr U inputRestart  = std::numeric_limits<U>::max();
    constexpr T outputRestart = std::numeric_limits<T>::max();
    std::vector<T> output;
    output.reserve(input.size());
    for (auto byte : input)
    {
        output.push_back(byte == inputRestart ? outputRestart : static_cast<T>(byte));
    }
    return output;
}

enum class PrimitiveRestartMode
{
    Disabled,
    Enabled,
    EnabledSplitContent,
};

enum class IndicesMode
{
    Buffer,
    BufferAndOffset,
    ClientSideArray,
};

enum class RestartOption
{
    None,
    AtBegin,
    AtEnd,
    Degenerate,
};

using DrawElementsVariantsTestParams = std::tuple<angle::PlatformParameters,
                                                  GLenum,                // type
                                                  bool,                  // useFlat
                                                  PrimitiveRestartMode,  // primitiveRestartMode
                                                  bool,                  // drawStrip
                                                  IndicesMode,           // indicesMode
                                                  RestartOption,         // restartOption
                                                  bool>;                 // useProvokingVertexFirst

std::string DrawElementsVariantsTestPrint(
    const ::testing::TestParamInfo<DrawElementsVariantsTestParams> &paramsInfo)
{
    const auto &[platform, type, useFlat, primitiveRestartMode, drawStrip, indicesMode,
                 restartOption, useProvokingVertexFirst] = paramsInfo.param;
    std::ostringstream out;
    out << platform << "__"
        << (type == GL_UNSIGNED_BYTE ? "B" : (type == GL_UNSIGNED_SHORT ? "S" : "I"))
        << (useFlat ? "_Flat" : "");
    switch (primitiveRestartMode)
    {
        case PrimitiveRestartMode::Disabled:
            break;
        case PrimitiveRestartMode::Enabled:
            out << "_Restart";
            break;
        case PrimitiveRestartMode::EnabledSplitContent:
            out << "_Split";
            break;
    }
    out << (drawStrip ? "_Strip" : "_Tris");
    switch (indicesMode)
    {
        case IndicesMode::Buffer:
            break;
        case IndicesMode::BufferAndOffset:
            out << "_Offset";
            break;
        case IndicesMode::ClientSideArray:
            out << "_Client";
            break;
    }
    switch (restartOption)
    {
        case RestartOption::None:
            break;
        case RestartOption::AtBegin:
            out << "_AtBegin";
            break;
        case RestartOption::AtEnd:
            out << "_AtEnd";
            break;
        case RestartOption::Degenerate:
            out << "_Degen";
            break;
    }
    out << (useProvokingVertexFirst ? "_First" : "");
    return out.str();
}

class DrawElementsVariantsTest : public ANGLETest<DrawElementsVariantsTestParams>
{
  protected:
    DrawElementsVariantsTest()
    {
        setWindowWidth(64);
        setWindowHeight(64);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    GLenum type() const { return std::get<1>(GetParam()); }
    bool useFlat() const { return std::get<2>(GetParam()); }
    PrimitiveRestartMode primitiveRestartMode() const { return std::get<3>(GetParam()); }
    bool drawStrip() const { return std::get<4>(GetParam()); }
    IndicesMode indicesMode() const { return std::get<5>(GetParam()); }
    RestartOption restartOption() const { return std::get<6>(GetParam()); }
    bool addRestartAtBegin() const { return restartOption() == RestartOption::AtBegin; }
    bool addRestartAtEnd() const { return restartOption() == RestartOption::AtEnd; }
    bool addRestartDegenerate() const { return restartOption() == RestartOption::Degenerate; }
    bool useProvokingVertexFirst() const { return std::get<7>(GetParam()); }

    bool usePrimitiveRestart() const
    {
        return primitiveRestartMode() != PrimitiveRestartMode::Disabled;
    }
    bool splitContent() const
    {
        return primitiveRestartMode() == PrimitiveRestartMode::EnabledSplitContent;
    }
    bool addOffset() const { return indicesMode() == IndicesMode::BufferAndOffset; }
    bool useClientSideArrays() const { return indicesMode() == IndicesMode::ClientSideArray; }
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
    ASSERT_GL_NO_ERROR();

    // "If drawElements is called with a count greater than zero, and no WebGLBuffer is bound to the
    // ELEMENT_ARRAY_BUFFER binding point, an INVALID_OPERATION error is generated."
    glDrawElements(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // count == 0 so it's fine to have no element array buffer bound.
    glDrawElements(GL_TRIANGLES, 0, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();
}

// Test uploading part of an index buffer after deleting a vertex array
// previously used for DrawElements.
TEST_P(DrawElementsTest, DeleteVertexArrayAndUploadIndex)
{
    const auto &vertices = GetIndexedQuadVertices();
    const auto &indices  = GetQuadIndices();

    ANGLE_GL_PROGRAM(programDrawRed, essl3_shaders::vs::Simple(), essl3_shaders::fs::Red());
    glUseProgram(programDrawRed);

    GLint posLocation = glGetAttribLocation(programDrawRed, essl3_shaders::PositionAttrib());
    ASSERT_NE(-1, posLocation);

    GLuint vertexArray;
    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * indices.size(), indices.data(),
                 GL_STATIC_DRAW);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);

    glDeleteVertexArrays(1, &vertexArray);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);

    // Could crash here if the observer binding from the vertex array doesn't get
    // removed on vertex array destruction.
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(indices[0]) * 3, indices.data());

    ASSERT_GL_NO_ERROR();
}

// Test VAO switch is handling cached element array buffer properly along with line loop mode
// switch.
TEST_P(DrawElementsTest, LineLoopTriangles)
{
    const auto &vertices                    = GetIndexedQuadVertices();
    constexpr std::array<GLuint, 6> indices = {{0, 1, 2, 0, 2, 3}};

    ANGLE_GL_PROGRAM(programDrawRed, essl3_shaders::vs::Simple(), essl3_shaders::fs::Red());
    ANGLE_GL_PROGRAM(programDrawBlue, essl3_shaders::vs::Simple(), essl3_shaders::fs::Blue());

    glUseProgram(programDrawRed);
    GLint posLocation = glGetAttribLocation(programDrawRed, essl3_shaders::PositionAttrib());
    ASSERT_NE(-1, posLocation);

    GLVertexArray vertexArray[2];
    GLBuffer vertexBuffer[2];

    glBindVertexArray(vertexArray[0]);

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * indices.size(), indices.data(),
                 GL_STATIC_DRAW);

    for (int i = 0; i < 2; i++)
    {
        glBindVertexArray(vertexArray[i]);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer[i]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                     GL_STATIC_DRAW);
        glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(posLocation);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    }

    // First draw with VAO0 and line loop mode
    glBindVertexArray(vertexArray[0]);
    glDrawArrays(GL_LINE_LOOP, 0, 4);

    // Switch to VAO1 and draw with triangle mode.
    glBindVertexArray(vertexArray[1]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(0, getWindowHeight() - 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::red);

    // Switch back to VAO0 and draw with triangle mode.
    glUseProgram(programDrawBlue);
    glBindVertexArray(vertexArray[0]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, 0, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(0, getWindowHeight() - 1, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::blue);
    ASSERT_GL_NO_ERROR();
}

// Regression test for using two VAOs, one to draw only GL_LINE_LOOPs, and
// another to draw indexed triangles.
TEST_P(DrawElementsTest, LineLoopTriangles2)
{
    const auto &vertices                    = GetIndexedQuadVertices();
    constexpr std::array<GLuint, 6> indices = {{0, 1, 2, 0, 2, 3}};

    ANGLE_GL_PROGRAM(programDrawRed, essl3_shaders::vs::Simple(), essl3_shaders::fs::Red());

    glUseProgram(programDrawRed);
    GLint posLocation = glGetAttribLocation(programDrawRed, essl3_shaders::PositionAttrib());
    ASSERT_NE(-1, posLocation);

    GLVertexArray vertexArray[2];
    GLBuffer vertexBuffer[2];

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * indices.size(), indices.data(),
                 GL_STATIC_DRAW);

    for (int i = 0; i < 2; i++)
    {
        glBindVertexArray(vertexArray[i]);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer[i]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                     GL_STATIC_DRAW);
        glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(posLocation);
        if (i != 0)
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    }

    // First draw with VAO0 and line loop mode
    glBindVertexArray(vertexArray[0]);
    glDrawArrays(GL_LINE_LOOP, 0, 4);

    // Switch to VAO1 and draw some indexed triangles
    glBindVertexArray(vertexArray[1]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    // Switch back to VAO0 and do another line loop
    glBindVertexArray(vertexArray[0]);

    // Would crash if the index buffer dirty bit got errantly set on VAO0.
    glDrawArrays(GL_LINE_LOOP, 0, 4);

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

// Verify that detaching shaders after linking doesn't break draw calls
TEST_P(DrawElementsTest, DrawWithDetachedShaders)
{
    const auto &vertices = GetIndexedQuadVertices();

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    GLBuffer indexBuffer;
    const auto &indices = GetQuadIndices();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * indices.size(), indices.data(),
                 GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    GLuint vertexShader   = CompileShader(GL_VERTEX_SHADER, essl3_shaders::vs::Simple());
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, essl3_shaders::fs::Red());
    ASSERT_NE(0u, vertexShader);
    ASSERT_NE(0u, fragmentShader);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program);

    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_EQ(GL_TRUE, linkStatus);

    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    ASSERT_GL_NO_ERROR();

    glUseProgram(program);

    GLint posLocation = glGetAttribLocation(program, essl3_shaders::PositionAttrib());
    ASSERT_NE(-1, posLocation);

    GLVertexArray vertexArray;
    glBindVertexArray(vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    ASSERT_GL_NO_ERROR();

    glDeleteProgram(program);
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

// Test that glDrawElements call with different index buffer offsets work as expected
TEST_P(DrawElementsTest, DrawElementsWithDifferentIndexBufferOffsets)
{
    const std::array<Vector3, 4> &vertices = GetIndexedQuadVertices();
    const std::array<GLushort, 6> &indices = GetQuadIndices();

    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    ANGLE_GL_PROGRAM(programDrawRed, essl3_shaders::vs::Simple(), essl3_shaders::fs::Red());
    ANGLE_GL_PROGRAM(programDrawGreen, essl3_shaders::vs::Simple(), essl3_shaders::fs::Green());
    ANGLE_GL_PROGRAM(programDrawBlue, essl3_shaders::vs::Simple(), essl3_shaders::fs::Blue());

    glUseProgram(programDrawRed);

    GLuint vertexArray;
    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);
    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    GLint posLocation = glGetAttribLocation(programDrawRed, essl3_shaders::PositionAttrib());
    ASSERT_NE(-1, posLocation);
    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Draw both triangles of quad
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices.data());
    EXPECT_PIXEL_COLOR_EQ(0, 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 2, GLColor::red);

    glUseProgram(programDrawGreen);

    GLuint vertexArray1;
    glGenVertexArrays(1, &vertexArray1);
    glBindVertexArray(vertexArray1);
    GLBuffer vertexBuffer1;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer1);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    posLocation = glGetAttribLocation(programDrawGreen, essl3_shaders::PositionAttrib());
    ASSERT_NE(-1, posLocation);
    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * indices.size(), indices.data(),
                 GL_DYNAMIC_DRAW);

    // Draw right triangle of quad
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>(6));
    EXPECT_PIXEL_COLOR_EQ(0, 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 2, GLColor::green);

    glUseProgram(programDrawBlue);

    glBindVertexArray(vertexArray);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    posLocation = glGetAttribLocation(programDrawBlue, essl3_shaders::PositionAttrib());
    ASSERT_NE(-1, posLocation);
    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);

    // Draw both triangles of quad
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices.data());
    EXPECT_PIXEL_COLOR_EQ(0, 1, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 2, GLColor::blue);

    glDeleteVertexArrays(1, &vertexArray);
    glDeleteVertexArrays(1, &vertexArray1);

    ASSERT_GL_NO_ERROR();
}

// Test one element buffer bind to two vertexArrays and switch vertexArray should draw correctly
TEST_P(DrawElementsTest, TwoVertexArraysWithSameElementBuffer)
{
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    ANGLE_GL_PROGRAM(programDrawRed, essl3_shaders::vs::Simple(), essl3_shaders::fs::Red());
    glUseProgram(programDrawRed);
    GLint posLocation = glGetAttribLocation(programDrawRed, essl3_shaders::PositionAttrib());
    ASSERT_NE(-1, posLocation);

    GLBuffer vertexBuffer;
    const std::array<Vector3, 4> &vertices = GetIndexedQuadVertices();
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    GLBuffer elementBuffer;
    std::array<GLubyte, 6> zeros;
    size_t elementBufferSize = sizeof(zeros[0]) * zeros.size();
    zeros.fill(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, elementBufferSize, zeros.data(), GL_DYNAMIC_DRAW);

    // Set up two vertex arrays using same set of buffers. Since initial element buffer all point to
    // the same vertex, it should only draw one point
    GLuint vertexArray[2];
    glGenVertexArrays(2, vertexArray);

    glBindVertexArray(vertexArray[0]);
    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, reinterpret_cast<const void *>(0));

    glBindVertexArray(vertexArray[1]);
    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, reinterpret_cast<const void *>(0));

    // Use vertexArray[0] and update elementBuffer and draw
    const std::array<GLubyte, 6> &indices = {0, 1, 2, 0, 2, 3};
    glBindVertexArray(vertexArray[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, elementBufferSize, indices.data(), GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, reinterpret_cast<const void *>(0));

    // Use vertexArray[1] and and draw
    glBindVertexArray(vertexArray[1]);
    size_t elementBufferOffset = sizeof(indices[0]) * 3;
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE,
                   reinterpret_cast<const void *>(elementBufferOffset));

    // We should see both triangles
    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::red);

    glDeleteVertexArrays(2, vertexArray);

    ASSERT_GL_NO_ERROR();
}

// Test that drawing with index value a little bit less than or equal to GL_MAX_ELEMENT_INDEX
// should not have error
TEST_P(DrawElementsTest, MaxElementIndex)
{
    constexpr GLuint indicesNumber = 10;
    std::array<GLuint, indicesNumber> indices;
    GLint64 maxIndex = 0;
    GLBuffer vertexBuffer;

    ANGLE_GL_PROGRAM(programDrawRed, essl3_shaders::vs::Simple(), essl3_shaders::fs::Red());
    glUseProgram(programDrawRed);
    GLint posLocation = glGetAttribLocation(programDrawRed, essl3_shaders::PositionAttrib());
    ASSERT_NE(-1, posLocation);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

    glEnableVertexAttribArray(posLocation);
    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glVertexAttribDivisor(posLocation, 0xFFFFFFFF);

    glGetInteger64v(GL_MAX_ELEMENT_INDEX, &maxIndex);
    // Draw using index from maxIndex - 9 to maxIndex. Should have no error
    for (GLuint i = 0; i < indicesNumber; ++i)
    {
        indices[i] = static_cast<GLuint>(maxIndex) - (indicesNumber - 1) + i;
    }

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indicesNumber), GL_UNSIGNED_INT,
                   indices.data());
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

    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, nullptr);
    ASSERT_GL_NO_ERROR();

    const GLushort indices2[] = {0, 0, 0, 0, 0, 0};
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices2), indices2, GL_STATIC_DRAW);

    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>(1));
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that subsequent UNSIGNED_BYTE primitive restart draws work without problems.
// At the time of writing, Metal backend would use incorrect internal offset and
// fail the draw with validation.
TEST_P(DrawElementsTest, DrawElementsUintByteBytePrimitiveRestart)
{
    glClearColor(1.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    const char kVS[] = R"(#version 300 es
in vec4 a_position;
void main()
{
    gl_Position = a_position;
    gl_PointSize = 2.0;
})";
    ANGLE_GL_PROGRAM(mProgram, kVS, essl3_shaders::fs::Green());
    glUseProgram(mProgram);

    GLBuffer vertexBuffer;
    std::array<GLfloat, 3> vertices{-1.0f, -1.0f, 0.5f};
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    GLint posLocation = glGetAttribLocation(mProgram, essl3_shaders::PositionAttrib());
    ASSERT_NE(-1, posLocation);
    glEnableVertexAttribArray(posLocation);
    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    GLubyte indices[4] = {0xff, 0xff, 0, 0};
    GLBuffer elementBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glDrawElements(GL_POINTS, 1, GL_UNSIGNED_SHORT, nullptr);
    ASSERT_GL_NO_ERROR();
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_RECT_EQ(1, 1, getWindowWidth() - 1, getWindowHeight() - 1, GLColor::red);
    ASSERT_GL_NO_ERROR();
}

// Tests various draw element parameter, vertex buffer contents variants.
// Does not yet test using GL_BYTE 0xFF, GL_SHORT 0xFFFF with primitive restart off.
TEST_P(DrawElementsVariantsTest, Draw)
{
    const bool hasProvokingVertexExt = IsGLExtensionEnabled("GL_ANGLE_provoking_vertex");
    ANGLE_SKIP_TEST_IF(useProvokingVertexFirst() && !hasProvokingVertexExt);

    glClearColor(1.f, 0.f, 0.f, 1.f);

    constexpr char kFlatVS[] = R"(#version 300 es
flat out float p;
in vec4 a_position;
in float a_vertexMark;
void main()
{
    p = a_vertexMark;
    gl_Position = a_position;
})";

    constexpr char kFlatFS[] = R"(#version 300 es
precision highp float;
flat in highp float p;
out vec4 my_FragColor;
void main()
{
    if (p >= 0.5)
        my_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
    else
        my_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
})";
    GLProgram program;
    if (useFlat())
    {
        program.makeRaster(kFlatVS, kFlatFS);
    }
    else
    {
        program.makeRaster(essl3_shaders::vs::Simple(), essl3_shaders::fs::Green());
    }
    ASSERT_GL_NO_ERROR();
    glUseProgram(program);

    GLBuffer vertexBuffer;
    std::array<GLfloat, 12> vertices = {
        -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
    };
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    // First vertex convention, triangle strip {0,1,2,3}: 0, 1 are provoking.
    GLBuffer markFirstConventionStrip;
    std::array<GLfloat, 4> markFirstStripData = {1.0f, 1.0f, 0.0f, 0.0f};
    glBindBuffer(GL_ARRAY_BUFFER, markFirstConventionStrip);
    glBufferData(GL_ARRAY_BUFFER, sizeof(markFirstStripData), markFirstStripData.data(),
                 GL_STATIC_DRAW);

    // First vertex convention, triangle strip {0,1,2,0xff,2,1,3} and triangles {0,1,2,2,1,3}:
    // 0, 2 are provoking.
    GLBuffer markFirstConventionTriangles;
    std::array<GLfloat, 4> markFirstTriData = {1.0f, 0.0f, 1.0f, 0.0f};
    glBindBuffer(GL_ARRAY_BUFFER, markFirstConventionTriangles);
    glBufferData(GL_ARRAY_BUFFER, sizeof(markFirstTriData), markFirstTriData.data(),
                 GL_STATIC_DRAW);

    // Last vertex convention: triangle strip {0,1,2,3} and triangles {0,1,2,2,1,3}: 2, 3 are
    // provoking.
    GLBuffer markLastConvention;
    std::array<GLfloat, 4> markLastData = {0.0f, 0.0f, 1.0f, 1.0f};
    glBindBuffer(GL_ARRAY_BUFFER, markLastConvention);
    glBufferData(GL_ARRAY_BUFFER, sizeof(markLastData), markLastData.data(), GL_STATIC_DRAW);

    GLubyte contentStrip[]     = {0, 1, 2, 3};
    GLubyte contentTriangles[] = {0, 1, 2, 2, 1, 3};
    GLubyte contentSplit[]     = {0, 1, 2, 0xff, 2, 1, 3};

    if (usePrimitiveRestart())
    {
        glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    }
    if (useProvokingVertexFirst())
    {
        glProvokingVertexANGLE(GL_FIRST_VERTEX_CONVENTION_ANGLE);
    }

    GLint posLocation = glGetAttribLocation(program, essl3_shaders::PositionAttrib());
    ASSERT_NE(-1, posLocation);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glEnableVertexAttribArray(posLocation);
    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);

    if (useFlat())
    {
        GLint markLocation = glGetAttribLocation(program, "a_vertexMark");
        ASSERT_NE(-1, markLocation);
        if (!useProvokingVertexFirst())
        {
            glBindBuffer(GL_ARRAY_BUFFER, markLastConvention);
        }
        else if (drawStrip() && !splitContent())
        {
            glBindBuffer(GL_ARRAY_BUFFER, markFirstConventionStrip);
        }
        else
        {
            glBindBuffer(GL_ARRAY_BUFFER, markFirstConventionTriangles);
        }
        glEnableVertexAttribArray(markLocation);
        glVertexAttribPointer(markLocation, 1, GL_FLOAT, GL_FALSE, 0, 0);
    }

    std::vector<GLubyte> byteIndices;
    GLsizei contentStartCount = 0;
    uintptr_t offset          = 0;
    if (addOffset())
    {
        byteIndices.push_back(0);
    }
    if (addRestartAtBegin())
    {
        byteIndices.push_back(0xff);
    }
    if (addRestartDegenerate())
    {
        byteIndices.push_back(0);
        byteIndices.push_back(0xff);
    }
    contentStartCount = byteIndices.size();
    if (splitContent())
    {
        byteIndices.insert(byteIndices.end(), std::begin(contentSplit), std::end(contentSplit));
    }
    else if (drawStrip())
    {
        byteIndices.insert(byteIndices.end(), std::begin(contentStrip), std::end(contentStrip));
    }
    else
    {
        byteIndices.insert(byteIndices.end(), std::begin(contentTriangles),
                           std::end(contentTriangles));
    }
    if (addRestartAtEnd())
    {
        byteIndices.push_back(0xff);
    }

    // Prepare typed index data for both buffer and client-side paths.
    std::vector<GLushort> shortIndices;
    std::vector<GLuint> intIndices;
    if (type() == GL_UNSIGNED_SHORT)
    {
        shortIndices = convertIndexBufferWithRestarts<GLushort>(byteIndices);
    }
    else if (type() == GL_UNSIGNED_INT)
    {
        intIndices = convertIndexBufferWithRestarts<GLuint>(byteIndices);
    }

    GLBuffer elementBuffer;
    const void *indices = nullptr;
    if (useClientSideArrays())
    {
        ASSERT_FALSE(addOffset());
        if (type() == GL_UNSIGNED_BYTE)
        {
            indices = byteIndices.data();
        }
        else if (type() == GL_UNSIGNED_SHORT)
        {
            indices = shortIndices.data();
        }
        else
        {
            indices = intIndices.data();
        }
    }
    else
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
        if (type() == GL_UNSIGNED_BYTE)
        {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, byteIndices.size(), byteIndices.data(),
                         GL_STATIC_DRAW);
            if (addOffset())
            {
                offset = 1;
            }
        }
        else if (type() == GL_UNSIGNED_SHORT)
        {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, 2 * shortIndices.size(), shortIndices.data(),
                         GL_STATIC_DRAW);
            if (addOffset())
            {
                offset = 2;
            }
        }
        else
        {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, 4 * intIndices.size(), intIndices.data(),
                         GL_STATIC_DRAW);
            if (addOffset())
            {
                offset = 4;
            }
        }
        indices = reinterpret_cast<const void *>(offset);
    }
    ASSERT_GL_NO_ERROR();

    glClear(GL_COLOR_BUFFER_BIT);

    GLenum mode   = drawStrip() ? GL_TRIANGLE_STRIP : GL_TRIANGLES;
    GLsizei count = byteIndices.size() - (addOffset() ? 1 : 0);
    glDrawElements(mode, count, type(), indices);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, getWindowHeight() - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green);
    ASSERT_GL_NO_ERROR();

    // Test drawing again to verify caching behavior and buffer regeneration.
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawElements(mode, count, type(), indices);
    glDrawElements(mode, count, type(), indices);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, getWindowHeight() - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green);
    ASSERT_GL_NO_ERROR();

    // Test drawing again with a subrange, to check that possibly cached ranges
    // are cropped correctly. Draw only one triangle.
    glClear(GL_COLOR_BUFFER_BIT);
    count = contentStartCount + 3 - (addOffset() ? 1 : 0);
    glDrawElements(mode, count, type(), indices);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(0, getWindowHeight() - 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::red);
    ASSERT_GL_NO_ERROR();
}

class WebGLDrawElementsTest3 : public WebGLDrawElementsTest
{};
// Test one element buffer bind to two vertexArrays and switch vertexArray should draw correctly.
// With WebGL, we will go through element buffer range validation check which will catch bugs if the
// cached index range is incorrect.
TEST_P(WebGLDrawElementsTest3, TwoVertexArraysWithSameElementBuffer)
{
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    ANGLE_GL_PROGRAM(programDrawRed, essl3_shaders::vs::Simple(), essl3_shaders::fs::Red());
    glUseProgram(programDrawRed);
    GLint posLocation = glGetAttribLocation(programDrawRed, essl3_shaders::PositionAttrib());
    ASSERT_NE(-1, posLocation);

    GLBuffer vertexBuffer;
    const std::array<Vector3, 4> &vertices = GetIndexedQuadVertices();
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    GLBuffer elementBuffer;
    std::array<GLuint, 6> invalidIndexData;
    size_t elementBufferSize = sizeof(invalidIndexData[0]) * invalidIndexData.size();
    invalidIndexData.fill(0xffffffff);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, elementBufferSize, invalidIndexData.data(),
                 GL_DYNAMIC_DRAW);

    // Set up two vertex arrays using same set of buffers. Since initial element buffer all point to
    // 0xffffffff, which exceed max index range, it should generate GL_INVALID_OPERATION.
    GLuint vertexArray[2];
    glGenVertexArrays(2, vertexArray);

    glBindVertexArray(vertexArray[0]);
    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, reinterpret_cast<const void *>(0));

    glBindVertexArray(vertexArray[1]);
    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
    size_t elementBufferOffset1 = sizeof(invalidIndexData[0]) * 3;
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT,
                   reinterpret_cast<const void *>(elementBufferOffset1));

    // This should alsop clear the context error code.
    ASSERT_TRUE(glGetError() == GL_INVALID_OPERATION);

    // Use vertexArray[0] and update elementBuffer and draw
    const std::array<GLuint, 6> &indices = {0, 1, 2, 0, 2, 3};
    glBindVertexArray(vertexArray[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, elementBufferSize, indices.data(), GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, reinterpret_cast<const void *>(0));

    // Use vertexArray[1] and and draw
    glBindVertexArray(vertexArray[1]);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT,
                   reinterpret_cast<const void *>(elementBufferOffset1));
    ASSERT_TRUE(glGetError() == GL_NO_ERROR);

    // We should see both triangles
    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::red);

    glDeleteVertexArrays(2, vertexArray);

    ASSERT_GL_NO_ERROR();
}

// Test that uploading a large index buffer (> 1MB) via glBufferSubData
// does not corrupt the data in the chunks after the first 1MB.
// This is a regression test for a bug in Metal backend where the offset
// was not applied to the source pointer when chunking the upload.
TEST_P(DrawElementsTest, LargeIndexBufferSubData)
{
    // We want the buffer size to be slightly larger than 1MB.
    // 1MB = 1024 * 1024 = 1,048,576 bytes.
    // With GL_UNSIGNED_INT (4 bytes), 1MB is 262,144 indices.
    // We use 263,168 indices (1,052,672 bytes).
    constexpr size_t kIndexCount = 263168;
    constexpr size_t kBufferSize = kIndexCount * sizeof(GLuint);

    // Staging buffer chunk size is 1MB.
    // First chunk: indices 0 to 262143.
    // Second chunk: indices 262144 to 263167.

    // Prepare indices.
    // We want to draw a triangle using the last 3 indices in the buffer.
    // These indices will point to green vertices.
    // The indices at the start of the buffer will point to red vertices.
    std::vector<GLuint> indices(kIndexCount, 0);
    // Start of buffer: points to red vertices (0, 1, 2)
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    // End of buffer: points to green vertices (3, 4, 5)
    indices[kIndexCount - 3] = 3;
    indices[kIndexCount - 2] = 4;
    indices[kIndexCount - 1] = 5;

    // Vertices with position and color.
    struct Vertex
    {
        float position[3];
        GLubyte color[4];
    };

    const std::vector<Vertex> vertices = {
        // Red triangle
        {{-1.0f, -1.0f, 0.0f}, {255, 0, 0, 255}},
        {{3.0f, -1.0f, 0.0f}, {255, 0, 0, 255}},
        {{-1.0f, 3.0f, 0.0f}, {255, 0, 0, 255}},
        // Green triangle
        {{-1.0f, -1.0f, 0.0f}, {0, 255, 0, 255}},
        {{3.0f, -1.0f, 0.0f}, {0, 255, 0, 255}},
        {{-1.0f, 3.0f, 0.0f}, {0, 255, 0, 255}},
    };

    // Shader program
    const char *kVS =
        "attribute vec3 a_position;\n"
        "attribute vec4 a_color;\n"
        "varying vec4 v_color;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_position, 1.0);\n"
        "    v_color = a_color;\n"
        "}\n";
    const char *kFS =
        "precision mediump float;\n"
        "varying vec4 v_color;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = v_color;\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    GLint posLocation   = glGetAttribLocation(program, "a_position");
    GLint colorLocation = glGetAttribLocation(program, "a_color");
    ASSERT_NE(-1, posLocation);
    ASSERT_NE(-1, colorLocation);

    GLVertexArray vao;
    glBindVertexArray(vao);

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void *>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(posLocation);
    glVertexAttribPointer(colorLocation, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
                          reinterpret_cast<const void *>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(colorLocation);

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    // Allocate
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, kBufferSize, nullptr, GL_STATIC_DRAW);
    // Upload > 1MB
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, kBufferSize, indices.data());

    ASSERT_GL_NO_ERROR();

    // Draw using the indices at the end of the buffer.
    // Offset in bytes: (kIndexCount - 3) * sizeof(GLuint)
    size_t drawOffset = (kIndexCount - 3) * sizeof(GLuint);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, reinterpret_cast<const void *>(drawOffset));

    ASSERT_GL_NO_ERROR();

    // If the bug is present, the indices at the end were copied from the start (which point to
    // red). If fixed, they point to green.
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::green);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DrawElementsTest);
ANGLE_INSTANTIATE_TEST_ES3(DrawElementsTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DrawElementsVariantsTest);

ANGLE_INSTANTIATE_TEST_VARIANTS_COMBINE_7(
    NoRestart,
    DrawElementsVariantsTest,
    DrawElementsVariantsTestPrint,
    testing::Values(GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, GL_UNSIGNED_INT),
    testing::Bool(),                                  // useFlat
    testing::Values(PrimitiveRestartMode::Disabled),  // primitiveRestartMode
    testing::Bool(),                                  // drawStrip
    testing::Values(IndicesMode::Buffer,
                    IndicesMode::BufferAndOffset,
                    IndicesMode::ClientSideArray),
    testing::Values(RestartOption::None),  // restartOption
    testing::Bool(),                       // useProvokingVertexFirst
    ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_VARIANTS_COMBINE_7(
    Restart,
    DrawElementsVariantsTest,
    DrawElementsVariantsTestPrint,
    testing::Values(GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, GL_UNSIGNED_INT),
    testing::Bool(),  // useFlat
    testing::Values(PrimitiveRestartMode::Enabled, PrimitiveRestartMode::EnabledSplitContent),
    testing::Bool(),  // drawStrip
    testing::Values(IndicesMode::Buffer,
                    IndicesMode::BufferAndOffset,
                    IndicesMode::ClientSideArray),
    testing::Values(RestartOption::None,
                    RestartOption::AtBegin,
                    RestartOption::AtEnd,
                    RestartOption::Degenerate),
    testing::Bool(),  // useProvokingVertexFirst
    ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_ES2(WebGLDrawElementsTest);
ANGLE_INSTANTIATE_TEST_ES3(WebGLDrawElementsTest3);
}  // namespace
