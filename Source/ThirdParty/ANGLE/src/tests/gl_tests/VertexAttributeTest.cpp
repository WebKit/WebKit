//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

using namespace angle;

namespace
{

GLsizei TypeStride(GLenum attribType)
{
    switch (attribType)
    {
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:
            return 1;
        case GL_UNSIGNED_SHORT:
        case GL_SHORT:
            return 2;
        case GL_UNSIGNED_INT:
        case GL_INT:
        case GL_FLOAT:
            return 4;
        default:
            UNREACHABLE();
            return 0;
    }
}

template <typename T>
GLfloat Normalize(T value)
{
    static_assert(std::is_integral<T>::value, "Integer required.");
    if (std::is_signed<T>::value)
    {
        typedef typename std::make_unsigned<T>::type unsigned_type;
        return (2.0f * static_cast<GLfloat>(value) + 1.0f) /
               static_cast<GLfloat>(std::numeric_limits<unsigned_type>::max());
    }
    else
    {
        return static_cast<GLfloat>(value) / static_cast<GLfloat>(std::numeric_limits<T>::max());
    }
}

class VertexAttributeTest : public ANGLETest
{
  protected:
    VertexAttributeTest()
        : mProgram(0), mTestAttrib(-1), mExpectedAttrib(-1), mBuffer(0), mQuadBuffer(0)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
    }

    enum class Source
    {
        BUFFER,
        IMMEDIATE,
    };

    struct TestData final : angle::NonCopyable
    {
        TestData(GLenum typeIn,
                 GLboolean normalizedIn,
                 Source sourceIn,
                 const void *inputDataIn,
                 const GLfloat *expectedDataIn)
            : type(typeIn),
              normalized(normalizedIn),
              bufferOffset(0),
              source(sourceIn),
              inputData(inputDataIn),
              expectedData(expectedDataIn)
        {
        }

        GLenum type;
        GLboolean normalized;
        size_t bufferOffset;
        Source source;

        const void *inputData;
        const GLfloat *expectedData;
    };

    void setupTest(const TestData &test, GLint typeSize)
    {
        if (mProgram == 0)
        {
            initBasicProgram();
        }

        if (test.source == Source::BUFFER)
        {
            GLsizei dataSize = mVertexCount * TypeStride(test.type) * typeSize;
            glBindBuffer(GL_ARRAY_BUFFER, mBuffer);
            glBufferData(GL_ARRAY_BUFFER, dataSize, test.inputData, GL_STATIC_DRAW);
            glVertexAttribPointer(mTestAttrib, typeSize, test.type, test.normalized, 0,
                                  reinterpret_cast<GLvoid *>(test.bufferOffset));
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        else
        {
            ASSERT_EQ(Source::IMMEDIATE, test.source);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glVertexAttribPointer(mTestAttrib, typeSize, test.type, test.normalized, 0,
                                  test.inputData);
        }

        glVertexAttribPointer(mExpectedAttrib, typeSize, GL_FLOAT, GL_FALSE, 0, test.expectedData);

        glEnableVertexAttribArray(mTestAttrib);
        glEnableVertexAttribArray(mExpectedAttrib);
    }

    void checkPixels()
    {
        GLint viewportSize[4];
        glGetIntegerv(GL_VIEWPORT, viewportSize);

        GLint midPixelX = (viewportSize[0] + viewportSize[2]) / 2;
        GLint midPixelY = (viewportSize[1] + viewportSize[3]) / 2;

        // We need to offset our checks from triangle edges to ensure we don't fall on a single tri
        // Avoid making assumptions of drawQuad with four checks to check the four possible tri
        // regions
        EXPECT_PIXEL_EQ((midPixelX + viewportSize[0]) / 2, midPixelY, 255, 255, 255, 255);
        EXPECT_PIXEL_EQ((midPixelX + viewportSize[2]) / 2, midPixelY, 255, 255, 255, 255);
        EXPECT_PIXEL_EQ(midPixelX, (midPixelY + viewportSize[1]) / 2, 255, 255, 255, 255);
        EXPECT_PIXEL_EQ(midPixelX, (midPixelY + viewportSize[3]) / 2, 255, 255, 255, 255);
    }

    void runTest(const TestData &test)
    {
        // TODO(geofflang): Figure out why this is broken on AMD OpenGL
        if (IsAMD() && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
        {
            std::cout << "Test skipped on AMD OpenGL." << std::endl;
            return;
        }

        for (GLint i = 0; i < 4; i++)
        {
            GLint typeSize = i + 1;
            setupTest(test, typeSize);

            drawQuad(mProgram, "position", 0.5f);

            glDisableVertexAttribArray(mTestAttrib);
            glDisableVertexAttribArray(mExpectedAttrib);

            checkPixels();
        }
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        glClearColor(0, 0, 0, 0);
        glClearDepthf(0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST);

        glGenBuffers(1, &mBuffer);
    }

    void TearDown() override
    {
        glDeleteProgram(mProgram);
        glDeleteBuffers(1, &mBuffer);
        glDeleteBuffers(1, &mQuadBuffer);

        ANGLETest::TearDown();
    }

    GLuint compileMultiAttribProgram(GLint attribCount)
    {
        std::stringstream shaderStream;

        shaderStream << "attribute mediump vec4 position;" << std::endl;
        for (GLint attribIndex = 0; attribIndex < attribCount; ++attribIndex)
        {
            shaderStream << "attribute float a" << attribIndex << ";" << std::endl;
        }
        shaderStream << "varying mediump float color;" << std::endl
                     << "void main() {" << std::endl
                     << "  gl_Position = position;" << std::endl
                     << "  color = 0.0;" << std::endl;
        for (GLint attribIndex = 0; attribIndex < attribCount; ++attribIndex)
        {
            shaderStream << "  color += a" << attribIndex << ";" << std::endl;
        }
        shaderStream << "}" << std::endl;

        const std::string testFragmentShaderSource =
            SHADER_SOURCE(varying mediump float color; void main(void)
                          {
                              gl_FragColor = vec4(color, 0.0, 0.0, 1.0);
                          });

        return CompileProgram(shaderStream.str(), testFragmentShaderSource);
    }

    void setupMultiAttribs(GLuint program, GLint attribCount, GLfloat value)
    {
        glUseProgram(program);
        for (GLint attribIndex = 0; attribIndex < attribCount; ++attribIndex)
        {
            std::stringstream attribStream;
            attribStream << "a" << attribIndex;
            GLint location = glGetAttribLocation(program, attribStream.str().c_str());
            ASSERT_NE(-1, location);
            glVertexAttrib1f(location, value);
            glDisableVertexAttribArray(location);
        }
    }

    void initBasicProgram()
    {
        const std::string testVertexShaderSource =
            "attribute mediump vec4 position;\n"
            "attribute mediump vec4 test;\n"
            "attribute mediump vec4 expected;\n"
            "varying mediump vec4 color;\n"
            "void main(void)\n"
            "{\n"
            "    gl_Position = position;\n"
            "    vec4 threshold = max(abs(expected) * 0.01, 1.0 / 64.0);\n"
            "    color = vec4(lessThanEqual(abs(test - expected), threshold));\n"
            "}\n";

        const std::string testFragmentShaderSource =
            "varying mediump vec4 color;\n"
            "void main(void)\n"
            "{\n"
            "    gl_FragColor = color;\n"
            "}\n";

        mProgram = CompileProgram(testVertexShaderSource, testFragmentShaderSource);
        ASSERT_NE(0u, mProgram);

        mTestAttrib = glGetAttribLocation(mProgram, "test");
        ASSERT_NE(-1, mTestAttrib);
        mExpectedAttrib = glGetAttribLocation(mProgram, "expected");
        ASSERT_NE(-1, mExpectedAttrib);

        glUseProgram(mProgram);
    }

    static const size_t mVertexCount = 24;

    GLuint mProgram;
    GLint mTestAttrib;
    GLint mExpectedAttrib;
    GLuint mBuffer;
    GLuint mQuadBuffer;
};

TEST_P(VertexAttributeTest, UnsignedByteUnnormalized)
{
    GLubyte inputData[mVertexCount] = { 0, 1, 2, 3, 4, 5, 6, 7, 125, 126, 127, 128, 129, 250, 251, 252, 253, 254, 255 };
    GLfloat expectedData[mVertexCount];
    for (size_t i = 0; i < mVertexCount; i++)
    {
        expectedData[i] = inputData[i];
    }

    TestData data(GL_UNSIGNED_BYTE, GL_FALSE, Source::IMMEDIATE, inputData, expectedData);
    runTest(data);
}

TEST_P(VertexAttributeTest, UnsignedByteNormalized)
{
    GLubyte inputData[mVertexCount] = { 0, 1, 2, 3, 4, 5, 6, 7, 125, 126, 127, 128, 129, 250, 251, 252, 253, 254, 255 };
    GLfloat expectedData[mVertexCount];
    for (size_t i = 0; i < mVertexCount; i++)
    {
        expectedData[i] = Normalize(inputData[i]);
    }

    TestData data(GL_UNSIGNED_BYTE, GL_TRUE, Source::IMMEDIATE, inputData, expectedData);
    runTest(data);
}

TEST_P(VertexAttributeTest, ByteUnnormalized)
{
    GLbyte inputData[mVertexCount] = { 0, 1, 2, 3, 4, -1, -2, -3, -4, 125, 126, 127, -128, -127, -126 };
    GLfloat expectedData[mVertexCount];
    for (size_t i = 0; i < mVertexCount; i++)
    {
        expectedData[i] = inputData[i];
    }

    TestData data(GL_BYTE, GL_FALSE, Source::IMMEDIATE, inputData, expectedData);
    runTest(data);
}

TEST_P(VertexAttributeTest, ByteNormalized)
{
    GLbyte inputData[mVertexCount] = { 0, 1, 2, 3, 4, -1, -2, -3, -4, 125, 126, 127, -128, -127, -126 };
    GLfloat expectedData[mVertexCount];
    for (size_t i = 0; i < mVertexCount; i++)
    {
        expectedData[i] = Normalize(inputData[i]);
    }

    TestData data(GL_BYTE, GL_TRUE, Source::IMMEDIATE, inputData, expectedData);
    runTest(data);
}

TEST_P(VertexAttributeTest, UnsignedShortUnnormalized)
{
    GLushort inputData[mVertexCount] = { 0, 1, 2, 3, 254, 255, 256, 32766, 32767, 32768, 65533, 65534, 65535 };
    GLfloat expectedData[mVertexCount];
    for (size_t i = 0; i < mVertexCount; i++)
    {
        expectedData[i] = inputData[i];
    }

    TestData data(GL_UNSIGNED_SHORT, GL_FALSE, Source::IMMEDIATE, inputData, expectedData);
    runTest(data);
}

TEST_P(VertexAttributeTest, UnsignedShortNormalized)
{
    GLushort inputData[mVertexCount] = { 0, 1, 2, 3, 254, 255, 256, 32766, 32767, 32768, 65533, 65534, 65535 };
    GLfloat expectedData[mVertexCount];
    for (size_t i = 0; i < mVertexCount; i++)
    {
        expectedData[i] = Normalize(inputData[i]);
    }

    TestData data(GL_UNSIGNED_SHORT, GL_TRUE, Source::IMMEDIATE, inputData, expectedData);
    runTest(data);
}

TEST_P(VertexAttributeTest, ShortUnnormalized)
{
    GLshort inputData[mVertexCount] = {  0, 1, 2, 3, -1, -2, -3, -4, 32766, 32767, -32768, -32767, -32766 };
    GLfloat expectedData[mVertexCount];
    for (size_t i = 0; i < mVertexCount; i++)
    {
        expectedData[i] = inputData[i];
    }

    TestData data(GL_SHORT, GL_FALSE, Source::IMMEDIATE, inputData, expectedData);
    runTest(data);
}

TEST_P(VertexAttributeTest, ShortNormalized)
{
    GLshort inputData[mVertexCount] = {  0, 1, 2, 3, -1, -2, -3, -4, 32766, 32767, -32768, -32767, -32766 };
    GLfloat expectedData[mVertexCount];
    for (size_t i = 0; i < mVertexCount; i++)
    {
        expectedData[i] = Normalize(inputData[i]);
    }

    TestData data(GL_SHORT, GL_TRUE, Source::IMMEDIATE, inputData, expectedData);
    runTest(data);
}

class VertexAttributeTestES3 : public VertexAttributeTest
{
  protected:
    VertexAttributeTestES3() {}
};

TEST_P(VertexAttributeTestES3, IntUnnormalized)
{
    GLint lo                      = std::numeric_limits<GLint>::min();
    GLint hi                      = std::numeric_limits<GLint>::max();
    GLint inputData[mVertexCount] = {0, 1, 2, 3, -1, -2, -3, -4, -1, hi, hi - 1, lo, lo + 1};
    GLfloat expectedData[mVertexCount];
    for (size_t i = 0; i < mVertexCount; i++)
    {
        expectedData[i] = static_cast<GLfloat>(inputData[i]);
    }

    TestData data(GL_INT, GL_FALSE, Source::BUFFER, inputData, expectedData);
    runTest(data);
}

TEST_P(VertexAttributeTestES3, IntNormalized)
{
    GLint lo                      = std::numeric_limits<GLint>::min();
    GLint hi                      = std::numeric_limits<GLint>::max();
    GLint inputData[mVertexCount] = {0, 1, 2, 3, -1, -2, -3, -4, -1, hi, hi - 1, lo, lo + 1};
    GLfloat expectedData[mVertexCount];
    for (size_t i = 0; i < mVertexCount; i++)
    {
        expectedData[i] = Normalize(inputData[i]);
    }

    TestData data(GL_INT, GL_TRUE, Source::BUFFER, inputData, expectedData);
    runTest(data);
}

TEST_P(VertexAttributeTestES3, UnsignedIntUnnormalized)
{
    GLuint mid                     = std::numeric_limits<GLuint>::max() >> 1;
    GLuint hi                      = std::numeric_limits<GLuint>::max();
    GLuint inputData[mVertexCount] = {0,       1,   2,       3,      254,    255, 256,
                                      mid - 1, mid, mid + 1, hi - 2, hi - 1, hi};
    GLfloat expectedData[mVertexCount];
    for (size_t i = 0; i < mVertexCount; i++)
    {
        expectedData[i] = static_cast<GLfloat>(inputData[i]);
    }

    TestData data(GL_UNSIGNED_INT, GL_FALSE, Source::BUFFER, inputData, expectedData);
    runTest(data);
}

TEST_P(VertexAttributeTestES3, UnsignedIntNormalized)
{
    GLuint mid                     = std::numeric_limits<GLuint>::max() >> 1;
    GLuint hi                      = std::numeric_limits<GLuint>::max();
    GLuint inputData[mVertexCount] = {0,       1,   2,       3,      254,    255, 256,
                                      mid - 1, mid, mid + 1, hi - 2, hi - 1, hi};
    GLfloat expectedData[mVertexCount];
    for (size_t i = 0; i < mVertexCount; i++)
    {
        expectedData[i] = Normalize(inputData[i]);
    }

    TestData data(GL_UNSIGNED_INT, GL_TRUE, Source::BUFFER, inputData, expectedData);
    runTest(data);
}

// Validate that we can support GL_MAX_ATTRIBS attribs
TEST_P(VertexAttributeTest, MaxAttribs)
{
    // TODO(jmadill): Figure out why we get this error on AMD/OpenGL and Intel.
    if ((IsIntel() || IsAMD()) && GetParam().getRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
    {
        std::cout << "Test skipped on Intel and AMD." << std::endl;
        return;
    }

    GLint maxAttribs;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttribs);
    ASSERT_GL_NO_ERROR();

    // Reserve one attrib for position
    GLint drawAttribs = maxAttribs - 1;

    GLuint program = compileMultiAttribProgram(drawAttribs);
    ASSERT_NE(0u, program);

    setupMultiAttribs(program, drawAttribs, 0.5f / static_cast<float>(drawAttribs));
    drawQuad(program, "position", 0.5f);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(0, 0, 128, 0, 0, 255, 1);
}

// Validate that we cannot support GL_MAX_ATTRIBS+1 attribs
TEST_P(VertexAttributeTest, MaxAttribsPlusOne)
{
    // TODO(jmadill): Figure out why we get this error on AMD/ES2/OpenGL
    if (IsAMD() && GetParam() == ES2_OPENGL())
    {
        std::cout << "Test disabled on AMD/ES2/OpenGL" << std::endl;
        return;
    }

    GLint maxAttribs;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttribs);
    ASSERT_GL_NO_ERROR();

    // Exceed attrib count by one (counting position)
    GLint drawAttribs = maxAttribs;

    GLuint program = compileMultiAttribProgram(drawAttribs);
    ASSERT_EQ(0u, program);
}

// Simple test for when we use glBindAttribLocation
TEST_P(VertexAttributeTest, SimpleBindAttribLocation)
{
    // TODO(jmadill): Figure out why this fails on Intel.
    if (IsIntel() && GetParam().getRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
    {
        std::cout << "Test skipped on Intel." << std::endl;
        return;
    }

    // Re-use the multi-attrib program, binding attribute 0
    GLuint program = compileMultiAttribProgram(1);
    glBindAttribLocation(program, 2, "position");
    glBindAttribLocation(program, 3, "a0");
    glLinkProgram(program);

    // Setup and draw the quad
    setupMultiAttribs(program, 1, 0.5f);
    drawQuad(program, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(0, 0, 128, 0, 0, 255, 1);
}

// Verify that drawing with a large out-of-range offset generates INVALID_OPERATION.
TEST_P(VertexAttributeTest, DrawArraysBufferTooSmall)
{
    GLfloat inputData[mVertexCount];
    GLfloat expectedData[mVertexCount];
    for (size_t count = 0; count < mVertexCount; ++count)
    {
        inputData[count]    = static_cast<GLfloat>(count);
        expectedData[count] = inputData[count];
    }

    TestData data(GL_FLOAT, GL_FALSE, Source::BUFFER, inputData, expectedData);
    data.bufferOffset = mVertexCount * TypeStride(GL_FLOAT);

    setupTest(data, 1);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Verify that index draw with an out-of-range offset generates INVALID_OPERATION.
TEST_P(VertexAttributeTest, DrawElementsBufferTooSmall)
{
    GLfloat inputData[mVertexCount];
    GLfloat expectedData[mVertexCount];
    for (size_t count = 0; count < mVertexCount; ++count)
    {
        inputData[count]    = static_cast<GLfloat>(count);
        expectedData[count] = inputData[count];
    }

    TestData data(GL_FLOAT, GL_FALSE, Source::BUFFER, inputData, expectedData);
    data.bufferOffset = (mVertexCount - 3) * TypeStride(GL_FLOAT);

    setupTest(data, 1);
    drawIndexedQuad(mProgram, "position", 0.5f);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Verify that using a different start vertex doesn't mess up the draw.
TEST_P(VertexAttributeTest, DrawArraysWithBufferOffset)
{
    // TODO(jmadill): Diagnose this failure.
    if (IsD3D11_FL93())
    {
        std::cout << "Test disabled on D3D11 FL 9_3" << std::endl;
        return;
    }

    // TODO(geofflang): Figure out why this is broken on AMD OpenGL
    if (IsAMD() && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
    {
        std::cout << "Test skipped on AMD OpenGL." << std::endl;
        return;
    }

    initBasicProgram();
    glUseProgram(mProgram);

    GLfloat inputData[mVertexCount];
    GLfloat expectedData[mVertexCount];
    for (size_t count = 0; count < mVertexCount; ++count)
    {
        inputData[count]    = static_cast<GLfloat>(count);
        expectedData[count] = inputData[count];
    }

    auto quadVertices        = GetQuadVertices();
    GLsizei quadVerticesSize = static_cast<GLsizei>(quadVertices.size() * sizeof(quadVertices[0]));

    glGenBuffers(1, &mQuadBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, mQuadBuffer);
    glBufferData(GL_ARRAY_BUFFER, quadVerticesSize + sizeof(Vector3), nullptr, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, quadVerticesSize, quadVertices.data());

    GLint positionLocation = glGetAttribLocation(mProgram, "position");
    ASSERT_NE(-1, positionLocation);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(positionLocation);

    GLsizei dataSize = mVertexCount * TypeStride(GL_FLOAT);
    glBindBuffer(GL_ARRAY_BUFFER, mBuffer);
    glBufferData(GL_ARRAY_BUFFER, dataSize + TypeStride(GL_FLOAT), nullptr, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, inputData);
    glVertexAttribPointer(mTestAttrib, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(mTestAttrib);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(mExpectedAttrib, 1, GL_FLOAT, GL_FALSE, 0, expectedData);
    glEnableVertexAttribArray(mExpectedAttrib);

    // Vertex draw with no start vertex offset (second argument is zero).
    glDrawArrays(GL_TRIANGLES, 0, 6);
    checkPixels();

    // Draw offset by one vertex.
    glDrawArrays(GL_TRIANGLES, 1, 6);
    checkPixels();

    EXPECT_GL_NO_ERROR();
}

class VertexAttributeCachingTest : public VertexAttributeTest
{
  protected:
    VertexAttributeCachingTest() {}

    void SetUp() override;

    template <typename DestT>
    static std::vector<GLfloat> GetExpectedData(const std::vector<GLubyte> &srcData,
                                                GLenum attribType,
                                                GLboolean normalized);

    void initDoubleAttribProgram()
    {
        const std::string testVertexShaderSource =
            "attribute mediump vec4 position;\n"
            "attribute mediump vec4 test;\n"
            "attribute mediump vec4 expected;\n"
            "attribute mediump vec4 test2;\n"
            "attribute mediump vec4 expected2;\n"
            "varying mediump vec4 color;\n"
            "void main(void)\n"
            "{\n"
            "    gl_Position = position;\n"
            "    vec4 threshold = max(abs(expected) * 0.01, 1.0 / 64.0);\n"
            "    color = vec4(lessThanEqual(abs(test - expected), threshold));\n"
            "    vec4 threshold2 = max(abs(expected2) * 0.01, 1.0 / 64.0);\n"
            "    color += vec4(lessThanEqual(abs(test2 - expected2), threshold2));\n"
            "}\n";

        const std::string testFragmentShaderSource =
            "varying mediump vec4 color;\n"
            "void main(void)\n"
            "{\n"
            "    gl_FragColor = color;\n"
            "}\n";

        mProgram = CompileProgram(testVertexShaderSource, testFragmentShaderSource);
        ASSERT_NE(0u, mProgram);

        mTestAttrib = glGetAttribLocation(mProgram, "test");
        ASSERT_NE(-1, mTestAttrib);
        mExpectedAttrib = glGetAttribLocation(mProgram, "expected");
        ASSERT_NE(-1, mExpectedAttrib);

        glUseProgram(mProgram);
    }

    struct AttribData
    {
        AttribData(GLenum typeIn, GLint sizeIn, GLboolean normalizedIn, GLsizei strideIn);

        GLenum type;
        GLint size;
        GLboolean normalized;
        GLsizei stride;
    };

    std::vector<AttribData> mTestData;
    std::map<GLenum, std::vector<GLfloat>> mExpectedData;
    std::map<GLenum, std::vector<GLfloat>> mNormExpectedData;
};

VertexAttributeCachingTest::AttribData::AttribData(GLenum typeIn,
                                                   GLint sizeIn,
                                                   GLboolean normalizedIn,
                                                   GLsizei strideIn)
    : type(typeIn), size(sizeIn), normalized(normalizedIn), stride(strideIn)
{
}

// static
template <typename DestT>
std::vector<GLfloat> VertexAttributeCachingTest::GetExpectedData(
    const std::vector<GLubyte> &srcData,
    GLenum attribType,
    GLboolean normalized)
{
    std::vector<GLfloat> expectedData;

    const DestT *typedSrcPtr = reinterpret_cast<const DestT *>(srcData.data());
    size_t iterations        = srcData.size() / TypeStride(attribType);

    if (normalized)
    {
        for (size_t index = 0; index < iterations; ++index)
        {
            expectedData.push_back(Normalize(typedSrcPtr[index]));
        }
    }
    else
    {
        for (size_t index = 0; index < iterations; ++index)
        {
            expectedData.push_back(static_cast<GLfloat>(typedSrcPtr[index]));
        }
    }

    return expectedData;
}

void VertexAttributeCachingTest::SetUp()
{
    VertexAttributeTest::SetUp();

    glBindBuffer(GL_ARRAY_BUFFER, mBuffer);

    std::vector<GLubyte> srcData;
    for (size_t count = 0; count < 4; ++count)
    {
        for (GLubyte i = 0; i < std::numeric_limits<GLubyte>::max(); ++i)
        {
            srcData.push_back(i);
        }
    }

    glBufferData(GL_ARRAY_BUFFER, srcData.size(), srcData.data(), GL_STATIC_DRAW);

    GLint viewportSize[4];
    glGetIntegerv(GL_VIEWPORT, viewportSize);

    std::vector<GLenum> attribTypes;
    attribTypes.push_back(GL_BYTE);
    attribTypes.push_back(GL_UNSIGNED_BYTE);
    attribTypes.push_back(GL_SHORT);
    attribTypes.push_back(GL_UNSIGNED_SHORT);

    if (getClientMajorVersion() >= 3)
    {
        attribTypes.push_back(GL_INT);
        attribTypes.push_back(GL_UNSIGNED_INT);
    }

    const GLint maxSize     = 4;
    const GLsizei maxStride = 4;

    for (GLenum attribType : attribTypes)
    {
        for (GLint attribSize = 1; attribSize <= maxSize; ++attribSize)
        {
            for (GLsizei stride = 1; stride <= maxStride; ++stride)
            {
                mTestData.push_back(AttribData(attribType, attribSize, GL_FALSE, stride));
                if (attribType != GL_FLOAT)
                {
                    mTestData.push_back(AttribData(attribType, attribSize, GL_TRUE, stride));
                }
            }
        }
    }

    mExpectedData[GL_BYTE]          = GetExpectedData<GLbyte>(srcData, GL_BYTE, GL_FALSE);
    mExpectedData[GL_UNSIGNED_BYTE] = GetExpectedData<GLubyte>(srcData, GL_UNSIGNED_BYTE, GL_FALSE);
    mExpectedData[GL_SHORT] = GetExpectedData<GLshort>(srcData, GL_SHORT, GL_FALSE);
    mExpectedData[GL_UNSIGNED_SHORT] =
        GetExpectedData<GLushort>(srcData, GL_UNSIGNED_SHORT, GL_FALSE);
    mExpectedData[GL_INT]          = GetExpectedData<GLint>(srcData, GL_INT, GL_FALSE);
    mExpectedData[GL_UNSIGNED_INT] = GetExpectedData<GLuint>(srcData, GL_UNSIGNED_INT, GL_FALSE);

    mNormExpectedData[GL_BYTE] = GetExpectedData<GLbyte>(srcData, GL_BYTE, GL_TRUE);
    mNormExpectedData[GL_UNSIGNED_BYTE] =
        GetExpectedData<GLubyte>(srcData, GL_UNSIGNED_BYTE, GL_TRUE);
    mNormExpectedData[GL_SHORT] = GetExpectedData<GLshort>(srcData, GL_SHORT, GL_TRUE);
    mNormExpectedData[GL_UNSIGNED_SHORT] =
        GetExpectedData<GLushort>(srcData, GL_UNSIGNED_SHORT, GL_TRUE);
    mNormExpectedData[GL_INT]          = GetExpectedData<GLint>(srcData, GL_INT, GL_TRUE);
    mNormExpectedData[GL_UNSIGNED_INT] = GetExpectedData<GLuint>(srcData, GL_UNSIGNED_INT, GL_TRUE);
}

// In D3D11, we must sometimes translate buffer data into static attribute caches. We also use a
// cache management scheme which garbage collects old attributes after we start using too much
// cache data. This test tries to make as many attribute caches from a single buffer as possible
// to stress-test the caching code.
TEST_P(VertexAttributeCachingTest, BufferMulticaching)
{
    if (IsAMD() && IsDesktopOpenGL())
    {
        std::cout << "Test skipped on AMD OpenGL." << std::endl;
        return;
    }

    initBasicProgram();

    glEnableVertexAttribArray(mTestAttrib);
    glEnableVertexAttribArray(mExpectedAttrib);

    ASSERT_GL_NO_ERROR();

    for (const auto &data : mTestData)
    {
        const auto &expected =
            (data.normalized) ? mNormExpectedData[data.type] : mExpectedData[data.type];

        GLsizei baseStride = static_cast<GLsizei>(data.size) * data.stride;
        GLsizei stride     = TypeStride(data.type) * baseStride;

        glBindBuffer(GL_ARRAY_BUFFER, mBuffer);
        glVertexAttribPointer(mTestAttrib, data.size, data.type, data.normalized, stride, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glVertexAttribPointer(mExpectedAttrib, data.size, GL_FLOAT, GL_FALSE,
                              sizeof(GLfloat) * baseStride, expected.data());
        drawQuad(mProgram, "position", 0.5f);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 255, 255, 255, 255);
    }
}

// With D3D11 dirty bits for VertxArray11, we can leave vertex state unchanged if there aren't any
// GL calls that affect it. This test targets leaving one vertex attribute unchanged between draw
// calls while changing another vertex attribute enough that it clears the static buffer cache
// after enough iterations. It validates the unchanged attributes don't get deleted incidentally.
TEST_P(VertexAttributeCachingTest, BufferMulticachingWithOneUnchangedAttrib)
{
    if (IsAMD() && IsDesktopOpenGL())
    {
        std::cout << "Test skipped on AMD OpenGL." << std::endl;
        return;
    }

    initDoubleAttribProgram();

    GLint testAttrib2Location = glGetAttribLocation(mProgram, "test2");
    ASSERT_NE(-1, testAttrib2Location);
    GLint expectedAttrib2Location = glGetAttribLocation(mProgram, "expected2");
    ASSERT_NE(-1, expectedAttrib2Location);

    glEnableVertexAttribArray(mTestAttrib);
    glEnableVertexAttribArray(mExpectedAttrib);
    glEnableVertexAttribArray(testAttrib2Location);
    glEnableVertexAttribArray(expectedAttrib2Location);

    ASSERT_GL_NO_ERROR();

    // Use an attribute that we know must be converted. This is a bit sensitive.
    glBindBuffer(GL_ARRAY_BUFFER, mBuffer);
    glVertexAttribPointer(testAttrib2Location, 3, GL_UNSIGNED_SHORT, GL_FALSE, 6, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(expectedAttrib2Location, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3,
                          mExpectedData[GL_UNSIGNED_SHORT].data());

    for (const auto &data : mTestData)
    {
        const auto &expected =
            (data.normalized) ? mNormExpectedData[data.type] : mExpectedData[data.type];

        GLsizei baseStride = static_cast<GLsizei>(data.size) * data.stride;
        GLsizei stride     = TypeStride(data.type) * baseStride;

        glBindBuffer(GL_ARRAY_BUFFER, mBuffer);
        glVertexAttribPointer(mTestAttrib, data.size, data.type, data.normalized, stride, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glVertexAttribPointer(mExpectedAttrib, data.size, GL_FLOAT, GL_FALSE,
                              sizeof(GLfloat) * baseStride, expected.data());
        drawQuad(mProgram, "position", 0.5f);

        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 255, 255, 255, 255);
    }
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
// D3D11 Feature Level 9_3 uses different D3D formats for vertex attribs compared to Feature Levels 10_0+, so we should test them separately.
ANGLE_INSTANTIATE_TEST(VertexAttributeTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES2_D3D11_FL9_3(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES());

ANGLE_INSTANTIATE_TEST(VertexAttributeTestES3, ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES());

ANGLE_INSTANTIATE_TEST(VertexAttributeCachingTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES3_OPENGL());

}  // anonymous namespace
