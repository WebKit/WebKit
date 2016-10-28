//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"
#include "random_utils.h"
#include "Vector.h"

using namespace angle;

namespace
{

class TransformFeedbackTest : public ANGLETest
{
  protected:
    TransformFeedbackTest()
        : mProgram(0),
          mTransformFeedbackBuffer(0),
          mTransformFeedback(0)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        glGenBuffers(1, &mTransformFeedbackBuffer);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBuffer);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBufferSize, NULL,
                     GL_STATIC_DRAW);

        glGenTransformFeedbacks(1, &mTransformFeedback);

        ASSERT_GL_NO_ERROR();
    }

    void TearDown() override
    {
        if (mProgram != 0)
        {
            glDeleteProgram(mProgram);
            mProgram = 0;
        }

        if (mTransformFeedbackBuffer != 0)
        {
            glDeleteBuffers(1, &mTransformFeedbackBuffer);
            mTransformFeedbackBuffer = 0;
        }

        if (mTransformFeedback != 0)
        {
            glDeleteTransformFeedbacks(1, &mTransformFeedback);
            mTransformFeedback = 0;
        }

        ANGLETest::TearDown();
    }

    void compileDefaultProgram(const std::vector<std::string> &tfVaryings, GLenum bufferMode)
    {
        ASSERT_EQ(0u, mProgram);

        const std::string vertexShaderSource = SHADER_SOURCE
        (
            precision highp float;
            attribute vec4 position;

            void main()
            {
                gl_Position = position;
            }
        );

        const std::string fragmentShaderSource = SHADER_SOURCE
        (
            precision highp float;

            void main()
            {
                gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
            }
        );

        mProgram = CompileProgramWithTransformFeedback(vertexShaderSource, fragmentShaderSource,
                                                       tfVaryings, bufferMode);
        ASSERT_NE(0u, mProgram);
    }

    GLuint mProgram;

    static const size_t mTransformFeedbackBufferSize = 1 << 24;
    GLuint mTransformFeedbackBuffer;
    GLuint mTransformFeedback;
};

TEST_P(TransformFeedbackTest, ZeroSizedViewport)
{
    // Set the program's transform feedback varyings (just gl_Position)
    std::vector<std::string> tfVaryings;
    tfVaryings.push_back("gl_Position");
    compileDefaultProgram(tfVaryings, GL_INTERLEAVED_ATTRIBS);

    glUseProgram(mProgram);

    // Bind the buffer for transform feedback output and start transform feedback
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mTransformFeedbackBuffer);
    glBeginTransformFeedback(GL_TRIANGLES);

    // Create a query to check how many primitives were written
    GLuint primitivesWrittenQuery = 0;
    glGenQueries(1, &primitivesWrittenQuery);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, primitivesWrittenQuery);

    // Set a viewport that would result in no pixels being written to the framebuffer and draw
    // a quad
    glViewport(0, 0, 0, 0);

    drawQuad(mProgram, "position", 0.5f);

    // End the query and transform feedkback
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    glEndTransformFeedback();

    glUseProgram(0);

    // Check how many primitives were written and verify that some were written even if
    // no pixels were rendered
    GLuint primitivesWritten = 0;
    glGetQueryObjectuiv(primitivesWrittenQuery, GL_QUERY_RESULT_EXT, &primitivesWritten);
    EXPECT_GL_NO_ERROR();

    EXPECT_EQ(2u, primitivesWritten);
}

// Test that rebinding a buffer with the same offset resets the offset (no longer appending from the
// old position)
TEST_P(TransformFeedbackTest, BufferRebinding)
{
    glDisable(GL_DEPTH_TEST);

    // Set the program's transform feedback varyings (just gl_Position)
    std::vector<std::string> tfVaryings;
    tfVaryings.push_back("gl_Position");
    compileDefaultProgram(tfVaryings, GL_INTERLEAVED_ATTRIBS);

    glUseProgram(mProgram);

    // Make sure the buffer has zero'd data
    std::vector<float> data(mTransformFeedbackBufferSize / sizeof(float), 0.0f);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBuffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBufferSize, data.data(),
                 GL_STATIC_DRAW);


    // Create a query to check how many primitives were written
    GLuint primitivesWrittenQuery = 0;
    glGenQueries(1, &primitivesWrittenQuery);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, primitivesWrittenQuery);

    const float finalZ = 0.95f;

    RNG rng;

    const size_t loopCount = 64;
    for (size_t loopIdx = 0; loopIdx < loopCount; loopIdx++)
    {
        // Bind the buffer for transform feedback output and start transform feedback
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mTransformFeedbackBuffer);
        glBeginTransformFeedback(GL_TRIANGLES);

        float z = (loopIdx + 1 == loopCount) ? finalZ : rng.randomFloatBetween(0.1f, 0.5f);
        drawQuad(mProgram, "position", z);

        glEndTransformFeedback();
    }

    // End the query and transform feedback
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);

    glUseProgram(0);

    // Check how many primitives were written and verify that some were written even if
    // no pixels were rendered
    GLuint primitivesWritten = 0;
    glGetQueryObjectuiv(primitivesWrittenQuery, GL_QUERY_RESULT_EXT, &primitivesWritten);
    EXPECT_GL_NO_ERROR();

    EXPECT_EQ(loopCount * 2, primitivesWritten);

    // Check the buffer data
    const float *bufferData = static_cast<float *>(glMapBufferRange(
        GL_TRANSFORM_FEEDBACK_BUFFER, 0, mTransformFeedbackBufferSize, GL_MAP_READ_BIT));

    for (size_t vertexIdx = 0; vertexIdx < 6; vertexIdx++)
    {
        // Check the third (Z) component of each vertex written and make sure it has the final
        // value
        EXPECT_NEAR(finalZ, bufferData[vertexIdx * 4 + 2], 0.0001);
    }

    for (size_t dataIdx = 24; dataIdx < mTransformFeedbackBufferSize / sizeof(float); dataIdx++)
    {
        EXPECT_EQ(data[dataIdx], bufferData[dataIdx]) << "Buffer overrun detected.";
    }

    glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);

    EXPECT_GL_NO_ERROR();
}

// Test that XFB can write back vertices to a buffer and that we can draw from this buffer afterward.
TEST_P(TransformFeedbackTest, RecordAndDraw)
{
    // TODO(jmadill): Figure out why this fails on Intel.
    if (IsIntel() && GetParam().getRenderer() == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        std::cout << "Test skipped on Intel." << std::endl;
        return;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set the program's transform feedback varyings (just gl_Position)
    std::vector<std::string> tfVaryings;
    tfVaryings.push_back("gl_Position");
    compileDefaultProgram(tfVaryings, GL_INTERLEAVED_ATTRIBS);

    glUseProgram(mProgram);

    GLint positionLocation = glGetAttribLocation(mProgram, "position");

    // First pass: draw 6 points to the XFB buffer
    glEnable(GL_RASTERIZER_DISCARD);

    const GLfloat vertices[] =
    {
        -1.0f,  1.0f, 0.5f,
        -1.0f, -1.0f, 0.5f,
         1.0f, -1.0f, 0.5f,

        -1.0f,  1.0f, 0.5f,
         1.0f, -1.0f, 0.5f,
         1.0f,  1.0f, 0.5f,
    };

    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(positionLocation);

    // Bind the buffer for transform feedback output and start transform feedback
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mTransformFeedbackBuffer);
    glBeginTransformFeedback(GL_POINTS);

    // Create a query to check how many primitives were written
    GLuint primitivesWrittenQuery = 0;
    glGenQueries(1, &primitivesWrittenQuery);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, primitivesWrittenQuery);

    glDrawArrays(GL_POINTS, 0, 6);

    glDisableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    // End the query and transform feedkback
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    glEndTransformFeedback();

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);

    glDisable(GL_RASTERIZER_DISCARD);

    // Check how many primitives were written and verify that some were written even if
    // no pixels were rendered
    GLuint primitivesWritten = 0;
    glGetQueryObjectuiv(primitivesWrittenQuery, GL_QUERY_RESULT_EXT, &primitivesWritten);
    EXPECT_GL_NO_ERROR();

    EXPECT_EQ(6u, primitivesWritten);

    // Nothing should have been drawn to the framebuffer
    EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 0, 0, 0, 0);

    // Second pass: draw from the feedback buffer

    glBindBuffer(GL_ARRAY_BUFFER, mTransformFeedbackBuffer);
    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(positionLocation);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 255,   0,   0, 255);
    EXPECT_GL_NO_ERROR();
}

// Test that buffer binding happens only on the current transform feedback object
TEST_P(TransformFeedbackTest, BufferBinding)
{
    // Reset any state
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

    // Generate a new buffer
    GLuint scratchBuffer = 0;
    glGenBuffers(1, &scratchBuffer);

    EXPECT_GL_NO_ERROR();

    // Bind TF 0 and a buffer
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBuffer);

    EXPECT_GL_NO_ERROR();

    // Check that the buffer ID matches the one that was just bound
    GLint currentBufferBinding = 0;
    glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &currentBufferBinding);
    EXPECT_EQ(static_cast<GLuint>(currentBufferBinding), mTransformFeedbackBuffer);

    EXPECT_GL_NO_ERROR();

    // Check that the buffer ID for the newly bound transform feedback is zero
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, mTransformFeedback);

    glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &currentBufferBinding);
    EXPECT_EQ(0, currentBufferBinding);

    EXPECT_GL_NO_ERROR();

    // Bind a buffer to this TF
    glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, scratchBuffer, 0, 32);

    glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &currentBufferBinding);
    EXPECT_EQ(static_cast<GLuint>(currentBufferBinding), scratchBuffer);

    EXPECT_GL_NO_ERROR();

    // Rebind the original TF and check it's bindings
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

    glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &currentBufferBinding);
    EXPECT_EQ(0, currentBufferBinding);

    EXPECT_GL_NO_ERROR();

    // Clean up
    glDeleteBuffers(1, &scratchBuffer);
}

// Test that we can capture varyings only used in the vertex shader.
TEST_P(TransformFeedbackTest, VertexOnly)
{
    const std::string &vertexShaderSource =
        "#version 300 es\n"
        "in vec2 position;\n"
        "in float attrib;\n"
        "out float varyingAttrib;\n"
        "void main() {\n"
        "  gl_Position = vec4(position, 0, 1);\n"
        "  varyingAttrib = attrib;\n"
        "}";

    const std::string &fragmentShaderSource =
        "#version 300 es\n"
        "out mediump vec4 color;\n"
        "void main() {\n"
        "  color = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "}";

    std::vector<std::string> tfVaryings;
    tfVaryings.push_back("varyingAttrib");

    mProgram = CompileProgramWithTransformFeedback(vertexShaderSource, fragmentShaderSource,
                                                   tfVaryings, GL_INTERLEAVED_ATTRIBS);
    ASSERT_NE(0u, mProgram);

    glUseProgram(mProgram);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, mTransformFeedback);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mTransformFeedbackBuffer);

    std::vector<float> attribData;
    for (unsigned int cnt = 0; cnt < 100; ++cnt)
    {
        attribData.push_back(static_cast<float>(cnt));
    }

    GLint attribLocation = glGetAttribLocation(mProgram, "attrib");
    ASSERT_NE(-1, attribLocation);

    glVertexAttribPointer(attribLocation, 1, GL_FLOAT, GL_FALSE, 4, &attribData[0]);
    glEnableVertexAttribArray(attribLocation);

    glBeginTransformFeedback(GL_TRIANGLES);
    drawQuad(mProgram, "position", 0.5f);
    glEndTransformFeedback();
    ASSERT_GL_NO_ERROR();

    glUseProgram(0);

    GLvoid *mappedBuffer =
        glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(float) * 6, GL_MAP_READ_BIT);
    ASSERT_NE(nullptr, mappedBuffer);

    float *mappedFloats = static_cast<float *>(mappedBuffer);
    for (unsigned int cnt = 0; cnt < 6; ++cnt)
    {
        EXPECT_EQ(attribData[cnt], mappedFloats[cnt]);
    }

    EXPECT_GL_NO_ERROR();
}

// Test that multiple paused transform feedbacks do not generate errors or crash
TEST_P(TransformFeedbackTest, MultiplePaused)
{
    const size_t drawSize = 1024;
    std::vector<float> transformFeedbackData(drawSize);
    for (size_t i = 0; i < drawSize; i++)
    {
        transformFeedbackData[i] = static_cast<float>(i + 1);
    }

    // Initialize the buffers to zero
    size_t bufferSize = drawSize;
    std::vector<float> bufferInitialData(bufferSize, 0);

    const size_t transformFeedbackCount = 8;

    // clang-format off
    const std::string vertexShaderSource = SHADER_SOURCE
    (  #version 300 es\n
       in highp vec4 position;
       in float transformFeedbackInput;
       out float transformFeedbackOutput;
       void main(void)
       {
           gl_Position = position;
           transformFeedbackOutput = transformFeedbackInput;
       }
    );

    const std::string fragmentShaderSource = SHADER_SOURCE
    (  #version 300 es\n
       out mediump vec4 color;
       void main(void)
       {
           color = vec4(1.0, 1.0, 1.0, 1.0);
       }
    );
    // clang-format on

    std::vector<std::string> tfVaryings;
    tfVaryings.push_back("transformFeedbackOutput");

    mProgram = CompileProgramWithTransformFeedback(vertexShaderSource, fragmentShaderSource,
                                                   tfVaryings, GL_INTERLEAVED_ATTRIBS);
    ASSERT_NE(0u, mProgram);
    glUseProgram(mProgram);

    GLint positionLocation = glGetAttribLocation(mProgram, "position");
    glDisableVertexAttribArray(positionLocation);
    glVertexAttrib4f(positionLocation, 0.0f, 0.0f, 0.0f, 1.0f);

    GLint tfInputLocation = glGetAttribLocation(mProgram, "transformFeedbackInput");
    glEnableVertexAttribArray(tfInputLocation);
    glVertexAttribPointer(tfInputLocation, 1, GL_FLOAT, false, 0, &transformFeedbackData[0]);

    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    ASSERT_GL_NO_ERROR();

    GLuint transformFeedbacks[transformFeedbackCount];
    glGenTransformFeedbacks(transformFeedbackCount, transformFeedbacks);

    GLuint buffers[transformFeedbackCount];
    glGenBuffers(transformFeedbackCount, buffers);

    for (size_t i = 0; i < transformFeedbackCount; i++)
    {
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedbacks[i]);

        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffers[i]);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, bufferSize * sizeof(GLfloat),
                     &bufferInitialData[0], GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffers[i]);
        ASSERT_GL_NO_ERROR();

        glBeginTransformFeedback(GL_POINTS);

        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(drawSize));

        glPauseTransformFeedback();

        EXPECT_GL_NO_ERROR();
    }

    for (size_t i = 0; i < transformFeedbackCount; i++)
    {
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedbacks[i]);
        glEndTransformFeedback();
        glDeleteTransformFeedbacks(1, &transformFeedbacks[i]);

        EXPECT_GL_NO_ERROR();
    }
}
// Test that running multiple simultaneous queries and transform feedbacks from multiple EGL
// contexts returns the correct results.  Helps expose bugs in ANGLE's virtual contexts.
TEST_P(TransformFeedbackTest, MultiContext)
{
#if defined(ANGLE_PLATFORM_APPLE)
    if ((IsNVIDIA() || IsAMD()) && GetParam() == ES3_OPENGL())
    {
        std::cout << "Test skipped on NVidia and AMD OpenGL on OSX." << std::endl;
        return;
    }
#endif

#if defined(ANGLE_PLATFORM_LINUX)
    if (IsAMD() && GetParam() == ES3_OPENGL())
    {
        std::cout << "Test skipped on AMD OpenGL on Linux." << std::endl;
        return;
    }
#endif

    EGLint contextAttributes[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR,
        GetParam().majorVersion,
        EGL_CONTEXT_MINOR_VERSION_KHR,
        GetParam().minorVersion,
        EGL_NONE,
    };

    EGLWindow *window = getEGLWindow();

    EGLDisplay display = window->getDisplay();
    EGLConfig config   = window->getConfig();
    EGLSurface surface = window->getSurface();

    const size_t passCount = 5;
    struct ContextInfo
    {
        EGLContext context;
        GLuint program;
        GLuint query;
        GLuint buffer;
        size_t primitiveCounts[passCount];
    };
    ContextInfo contexts[32];

    const size_t maxDrawSize = 1024;

    std::vector<float> transformFeedbackData(maxDrawSize);
    for (size_t i = 0; i < maxDrawSize; i++)
    {
        transformFeedbackData[i] = static_cast<float>(i + 1);
    }

    // Initialize the buffers to zero
    size_t bufferSize = maxDrawSize * passCount;
    std::vector<float> bufferInitialData(bufferSize, 0);

    for (auto &context : contexts)
    {
        context.context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
        ASSERT_NE(context.context, EGL_NO_CONTEXT);

        eglMakeCurrent(display, surface, surface, context.context);

        // clang-format off
        const std::string vertexShaderSource = SHADER_SOURCE
        (   #version 300 es\n
            in highp vec4 position;
            in float transformFeedbackInput;
            out float transformFeedbackOutput;
            void main(void)
            {
                gl_Position = position;
                transformFeedbackOutput = transformFeedbackInput;
            }
        );

        const std::string fragmentShaderSource = SHADER_SOURCE
        (   #version 300 es\n
            out mediump vec4 color;
            void main(void)
            {
                color = vec4(1.0, 1.0, 1.0, 1.0);
            }
        );
        // clang-format on

        std::vector<std::string> tfVaryings;
        tfVaryings.push_back("transformFeedbackOutput");

        context.program = CompileProgramWithTransformFeedback(
            vertexShaderSource, fragmentShaderSource, tfVaryings, GL_INTERLEAVED_ATTRIBS);
        ASSERT_NE(context.program, 0u);
        glUseProgram(context.program);

        GLint positionLocation = glGetAttribLocation(context.program, "position");
        glDisableVertexAttribArray(positionLocation);
        glVertexAttrib4f(positionLocation, 0.0f, 0.0f, 0.0f, 1.0f);

        GLint tfInputLocation = glGetAttribLocation(context.program, "transformFeedbackInput");
        glEnableVertexAttribArray(tfInputLocation);
        glVertexAttribPointer(tfInputLocation, 1, GL_FLOAT, false, 0, &transformFeedbackData[0]);

        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glGenQueriesEXT(1, &context.query);
        glBeginQueryEXT(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, context.query);

        ASSERT_GL_NO_ERROR();

        glGenBuffers(1, &context.buffer);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, context.buffer);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, bufferSize * sizeof(GLfloat),
                     &bufferInitialData[0], GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, context.buffer);

        ASSERT_GL_NO_ERROR();

        // For each pass, draw between 0 and maxDrawSize primitives
        for (auto &primCount : context.primitiveCounts)
        {
            primCount = rand() % maxDrawSize;
        }

        glBeginTransformFeedback(GL_POINTS);
    }

    for (size_t pass = 0; pass < passCount; pass++)
    {
        for (const auto &context : contexts)
        {
            eglMakeCurrent(display, surface, surface, context.context);

            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(context.primitiveCounts[pass]));
        }
    }

    for (const auto &context : contexts)
    {
        eglMakeCurrent(display, surface, surface, context.context);

        glEndTransformFeedback();

        glEndQueryEXT(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);

        GLuint result = 0;
        glGetQueryObjectuivEXT(context.query, GL_QUERY_RESULT_EXT, &result);

        EXPECT_GL_NO_ERROR();

        size_t totalPrimCount = 0;
        for (const auto &primCount : context.primitiveCounts)
        {
            totalPrimCount += primCount;
        }
        EXPECT_EQ(static_cast<GLuint>(totalPrimCount), result);

        const float *bufferData = reinterpret_cast<float *>(glMapBufferRange(
            GL_TRANSFORM_FEEDBACK_BUFFER, 0, bufferSize * sizeof(GLfloat), GL_MAP_READ_BIT));

        size_t curBufferIndex = 0;
        for (const auto &primCount : context.primitiveCounts)
        {
            for (size_t prim = 0; prim < primCount; prim++)
            {
                EXPECT_EQ(bufferData[curBufferIndex], prim + 1);
                curBufferIndex++;
            }
        }

        while (curBufferIndex < bufferSize)
        {
            EXPECT_EQ(bufferData[curBufferIndex], 0.0f);
            curBufferIndex++;
        }

        glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
    }

    eglMakeCurrent(display, surface, surface, window->getContext());

    for (auto &context : contexts)
    {
        eglDestroyContext(display, context.context);
        context.context = EGL_NO_CONTEXT;
    }
}

// Test that when two vec2s are packed into the same register, we can still capture both of them.
TEST_P(TransformFeedbackTest, PackingBug)
{
    // TODO(jmadill): With points and rasterizer discard?
    const std::string &vertexShaderSource =
        "#version 300 es\n"
        "in vec2 inAttrib1;\n"
        "in vec2 inAttrib2;\n"
        "out vec2 outAttrib1;\n"
        "out vec2 outAttrib2;\n"
        "in vec2 position;\n"
        "void main() {"
        "  outAttrib1 = inAttrib1;\n"
        "  outAttrib2 = inAttrib2;\n"
        "  gl_Position = vec4(position, 0, 1);\n"
        "}";

    const std::string &fragmentShaderSource =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 color;\n"
        "void main() {\n"
        "  color = vec4(0);\n"
        "}";

    std::vector<std::string> tfVaryings;
    tfVaryings.push_back("outAttrib1");
    tfVaryings.push_back("outAttrib2");

    mProgram = CompileProgramWithTransformFeedback(vertexShaderSource, fragmentShaderSource,
                                                   tfVaryings, GL_INTERLEAVED_ATTRIBS);
    ASSERT_NE(0u, mProgram);

    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBuffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof(Vector2) * 2 * 6, nullptr, GL_STREAM_DRAW);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, mTransformFeedback);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mTransformFeedbackBuffer);

    GLint attrib1Loc = glGetAttribLocation(mProgram, "inAttrib1");
    GLint attrib2Loc = glGetAttribLocation(mProgram, "inAttrib2");

    Vector2 attrib1Data[] = {Vector2(1.0, 2.0), Vector2(3.0, 4.0), Vector2(5.0, 6.0)};
    Vector2 attrib2Data[] = {Vector2(11.0, 12.0), Vector2(13.0, 14.0), Vector2(15.0, 16.0)};

    glEnableVertexAttribArray(attrib1Loc);
    glEnableVertexAttribArray(attrib2Loc);

    glVertexAttribPointer(attrib1Loc, 2, GL_FLOAT, GL_FALSE, 0, attrib1Data);
    glVertexAttribPointer(attrib2Loc, 2, GL_FLOAT, GL_FALSE, 0, attrib2Data);

    glUseProgram(mProgram);
    glBeginTransformFeedback(GL_TRIANGLES);
    drawQuad(mProgram, "position", 0.5f);
    glEndTransformFeedback();
    glUseProgram(0);
    ASSERT_GL_NO_ERROR();

    const GLvoid *mapPointer =
        glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(Vector2) * 2 * 6, GL_MAP_READ_BIT);
    ASSERT_NE(nullptr, mapPointer);

    const Vector2 *vecPointer = static_cast<const Vector2 *>(mapPointer);
    for (unsigned int vectorIndex = 0; vectorIndex < 3; ++vectorIndex)
    {
        unsigned int stream1Index = vectorIndex * 2;
        unsigned int stream2Index = vectorIndex * 2 + 1;
        EXPECT_EQ(attrib1Data[vectorIndex], vecPointer[stream1Index]);
        EXPECT_EQ(attrib2Data[vectorIndex], vecPointer[stream2Index]);
    }

    ASSERT_GL_NO_ERROR();
}

// Test that transform feedback varyings that can be optimized out yet do not cause program
// compilation to fail
TEST_P(TransformFeedbackTest, OptimizedVaryings)
{
    const std::string &vertexShaderSource =
        "#version 300 es\n"
        "in vec4 a_vertex;\n"
        "in vec3 a_normal; \n"
        "\n"
        "uniform Transform\n"
        "{\n"
        "    mat4 u_modelViewMatrix;\n"
        "    mat4 u_projectionMatrix;\n"
        "    mat3 u_normalMatrix;\n"
        "};\n"
        "\n"
        "out vec3 normal;\n"
        "out vec4 ecPosition;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    normal = normalize(u_normalMatrix * a_normal);\n"
        "    ecPosition = u_modelViewMatrix * a_vertex;\n"
        "    gl_Position = u_projectionMatrix * ecPosition;\n"
        "}\n";

    const std::string &fragmentShaderSource =
        "#version 300 es\n"
        "precision mediump float;\n"
        "\n"
        "in vec3 normal;\n"
        "in vec4 ecPosition;\n"
        "\n"
        "out vec4 fragColor;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    fragColor = vec4(normal/2.0+vec3(0.5), 1);\n"
        "}\n";

    std::vector<std::string> tfVaryings;
    tfVaryings.push_back("normal");
    tfVaryings.push_back("ecPosition");

    mProgram = CompileProgramWithTransformFeedback(vertexShaderSource, fragmentShaderSource,
                                                   tfVaryings, GL_INTERLEAVED_ATTRIBS);
    ASSERT_NE(0u, mProgram);
}

// Test an edge case where two varyings are unreferenced in the frag shader.
TEST_P(TransformFeedbackTest, TwoUnreferencedInFragShader)
{
    // TODO(jmadill): With points and rasterizer discard?
    const std::string &vertexShaderSource =
        "#version 300 es\n"
        "in vec3 position;\n"
        "out vec3 outAttrib1;\n"
        "out vec3 outAttrib2;\n"
        "void main() {"
        "  outAttrib1 = position;\n"
        "  outAttrib2 = position;\n"
        "  gl_Position = vec4(position, 1);\n"
        "}";

    const std::string &fragmentShaderSource =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 color;\n"
        "in vec3 outAttrib1;\n"
        "in vec3 outAttrib2;\n"
        "void main() {\n"
        "  color = vec4(0);\n"
        "}";

    std::vector<std::string> tfVaryings;
    tfVaryings.push_back("outAttrib1");
    tfVaryings.push_back("outAttrib2");

    mProgram = CompileProgramWithTransformFeedback(vertexShaderSource, fragmentShaderSource,
                                                   tfVaryings, GL_INTERLEAVED_ATTRIBS);
    ASSERT_NE(0u, mProgram);

    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBuffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof(Vector3) * 2 * 6, nullptr, GL_STREAM_DRAW);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, mTransformFeedback);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mTransformFeedbackBuffer);

    glUseProgram(mProgram);
    glBeginTransformFeedback(GL_TRIANGLES);
    drawQuad(mProgram, "position", 0.5f);
    glEndTransformFeedback();
    glUseProgram(0);
    ASSERT_GL_NO_ERROR();

    const GLvoid *mapPointer =
        glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(Vector2) * 2 * 6, GL_MAP_READ_BIT);
    ASSERT_NE(nullptr, mapPointer);

    const auto &quadVertices = GetQuadVertices();

    const Vector3 *vecPointer = static_cast<const Vector3 *>(mapPointer);
    for (unsigned int vectorIndex = 0; vectorIndex < 3; ++vectorIndex)
    {
        unsigned int stream1Index = vectorIndex * 2;
        unsigned int stream2Index = vectorIndex * 2 + 1;
        EXPECT_EQ(quadVertices[vectorIndex], vecPointer[stream1Index]);
        EXPECT_EQ(quadVertices[vectorIndex], vecPointer[stream2Index]);
    }

    ASSERT_GL_NO_ERROR();
}
class TransformFeedbackLifetimeTest : public TransformFeedbackTest
{
  protected:
    TransformFeedbackLifetimeTest() : mVertexArray(0) {}

    void SetUp() override
    {
        ANGLETest::SetUp();

        glGenVertexArrays(1, &mVertexArray);
        glBindVertexArray(mVertexArray);

        std::vector<std::string> tfVaryings;
        tfVaryings.push_back("gl_Position");
        compileDefaultProgram(tfVaryings, GL_SEPARATE_ATTRIBS);

        glGenBuffers(1, &mTransformFeedbackBuffer);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBuffer);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBufferSize, NULL,
                     GL_DYNAMIC_DRAW);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

        glGenTransformFeedbacks(1, &mTransformFeedback);

        ASSERT_GL_NO_ERROR();
    }

    void TearDown() override
    {
        glDeleteVertexArrays(1, &mVertexArray);
        TransformFeedbackTest::TearDown();
    }

    GLuint mVertexArray;
};

// Tests a bug with state syncing and deleted transform feedback buffers.
TEST_P(TransformFeedbackLifetimeTest, DeletedBuffer)
{
    // First stream vertex data to mTransformFeedbackBuffer.
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, mTransformFeedback);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mTransformFeedbackBuffer);

    glUseProgram(mProgram);

    glBeginTransformFeedback(GL_TRIANGLES);
    drawQuad(mProgram, "position", 0.5f, 1.0f, true);
    glEndTransformFeedback();

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

    // TODO(jmadill): Remove this when http://anglebug.com/1351 is fixed.
    glBindVertexArray(0);
    drawQuad(mProgram, "position", 0.5f);
    glBindVertexArray(1);

    // Next, draw vertices with mTransformFeedbackBuffer. This will link to mVertexArray.
    glBindBuffer(GL_ARRAY_BUFFER, mTransformFeedbackBuffer);
    GLint loc = glGetAttribLocation(mProgram, "position");
    ASSERT_NE(-1, loc);
    glVertexAttribPointer(loc, 1, GL_FLOAT, GL_FALSE, 4, nullptr);
    glEnableVertexAttribArray(loc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Delete resources, making a stranded pointer to mVertexArray in mTransformFeedbackBuffer.
    glDeleteBuffers(1, &mTransformFeedbackBuffer);
    mTransformFeedbackBuffer = 0;
    glDeleteVertexArrays(1, &mVertexArray);
    mVertexArray = 0;

    // Then draw again with transform feedback, dereferencing the stranded pointer.
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, mTransformFeedback);
    glBeginTransformFeedback(GL_TRIANGLES);
    drawQuad(mProgram, "position", 0.5f, 1.0f, true);
    glEndTransformFeedback();
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

    ASSERT_GL_NO_ERROR();
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
ANGLE_INSTANTIATE_TEST(TransformFeedbackTest, ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES());
ANGLE_INSTANTIATE_TEST(TransformFeedbackLifetimeTest, ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES());

}  // anonymous namespace
