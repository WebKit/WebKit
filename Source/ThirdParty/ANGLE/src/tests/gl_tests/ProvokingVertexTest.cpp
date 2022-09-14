//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProvkingVertexTest:
//   Tests on the conformance of the provoking vertex, which applies to flat
//   shading and compatibility with D3D. See the section on 'flatshading'
//   in the ES 3 specs.
//

#include "test_utils/ANGLETest.h"

using namespace angle;

namespace
{

class ProvokingVertexTest : public ANGLETest<>
{
  protected:
    ProvokingVertexTest()
        : mProgram(0),
          mFramebuffer(0),
          mTexture(0),
          mTransformFeedback(0),
          mBuffer(0),
          mIntAttribLocation(-1)
    {
        setWindowWidth(64);
        setWindowHeight(64);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
    }

    void testSetUp() override
    {
        constexpr char kVS[] =
            "#version 300 es\n"
            "in int intAttrib;\n"
            "in vec2 position;\n"
            "flat out int attrib;\n"
            "void main() {\n"
            "  gl_Position = vec4(position, 0, 1);\n"
            "  attrib = intAttrib;\n"
            "}";

        constexpr char kFS[] =
            "#version 300 es\n"
            "flat in int attrib;\n"
            "out int fragColor;\n"
            "void main() {\n"
            "  fragColor = attrib;\n"
            "}";

        std::vector<std::string> tfVaryings;
        tfVaryings.push_back("attrib");
        mProgram = CompileProgramWithTransformFeedback(kVS, kFS, tfVaryings, GL_SEPARATE_ATTRIBS);
        ASSERT_NE(0u, mProgram);

        glGenTextures(1, &mTexture);
        glBindTexture(GL_TEXTURE_2D, mTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32I, getWindowWidth(), getWindowHeight());

        glGenFramebuffers(1, &mFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture, 0);

        mIntAttribLocation = glGetAttribLocation(mProgram, "intAttrib");
        ASSERT_NE(-1, mIntAttribLocation);
        glEnableVertexAttribArray(mIntAttribLocation);

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        if (mProgram != 0)
        {
            glDeleteProgram(mProgram);
            mProgram = 0;
        }

        if (mFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &mFramebuffer);
            mFramebuffer = 0;
        }

        if (mTexture != 0)
        {
            glDeleteTextures(1, &mTexture);
            mTexture = 0;
        }

        if (mTransformFeedback != 0)
        {
            glDeleteTransformFeedbacks(1, &mTransformFeedback);
            mTransformFeedback = 0;
        }

        if (mBuffer != 0)
        {
            glDeleteBuffers(1, &mBuffer);
            mBuffer = 0;
        }
    }

    GLuint mProgram;
    GLuint mFramebuffer;
    GLuint mTexture;
    GLuint mTransformFeedback;
    GLuint mBuffer;
    GLint mIntAttribLocation;
};

// Test drawing a simple triangle with flat shading, and different valued vertices.
TEST_P(ProvokingVertexTest, FlatTriangle)
{
    GLint vertexData[] = {1, 2, 3, 1, 2, 3};
    glVertexAttribIPointer(mIntAttribLocation, 1, GL_INT, 0, vertexData);

    drawQuad(mProgram, "position", 0.5f);

    GLint pixelValue[4] = {0};
    glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_INT, &pixelValue);

    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(vertexData[2], pixelValue[0]);
}

// Ensure that any provoking vertex shenanigans still gives correct vertex streams.
TEST_P(ProvokingVertexTest, FlatTriWithTransformFeedback)
{
    // TODO(cwallez) figure out why it is broken on AMD on Mac
    ANGLE_SKIP_TEST_IF(IsOSX() && IsAMD());

    glGenTransformFeedbacks(1, &mTransformFeedback);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, mTransformFeedback);

    glGenBuffers(1, &mBuffer);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, mBuffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 128, nullptr, GL_STREAM_DRAW);

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mBuffer);

    GLint vertexData[] = {1, 2, 3, 1, 2, 3};
    glVertexAttribIPointer(mIntAttribLocation, 1, GL_INT, 0, vertexData);

    glUseProgram(mProgram);
    glBeginTransformFeedback(GL_TRIANGLES);
    drawQuad(mProgram, "position", 0.5f);
    glEndTransformFeedback();
    glUseProgram(0);

    GLint pixelValue[4] = {0};
    glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_INT, &pixelValue);

    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(vertexData[2], pixelValue[0]);

    void *mapPointer =
        glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(int) * 6, GL_MAP_READ_BIT);
    ASSERT_NE(nullptr, mapPointer);

    int *mappedInts = static_cast<int *>(mapPointer);
    for (unsigned int cnt = 0; cnt < 6; ++cnt)
    {
        EXPECT_EQ(vertexData[cnt], mappedInts[cnt]);
    }
}

// Test drawing a simple line with flat shading, and different valued vertices.
TEST_P(ProvokingVertexTest, FlatLine)
{
    GLfloat halfPixel = 1.0f / static_cast<GLfloat>(getWindowWidth());

    GLint vertexData[]     = {1, 2};
    GLfloat positionData[] = {-1.0f + halfPixel, -1.0f, -1.0f + halfPixel, 1.0f};

    glVertexAttribIPointer(mIntAttribLocation, 1, GL_INT, 0, vertexData);

    GLint positionLocation = glGetAttribLocation(mProgram, "position");
    glEnableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, positionData);

    glUseProgram(mProgram);
    glDrawArrays(GL_LINES, 0, 2);

    GLint pixelValue[4] = {0};
    glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_INT, &pixelValue);

    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(vertexData[1], pixelValue[0]);
}

// Test drawing a simple line with flat shading, and different valued vertices.
TEST_P(ProvokingVertexTest, FlatLineWithFirstIndex)
{
    GLfloat halfPixel = 1.0f / static_cast<GLfloat>(getWindowWidth());

    GLint vertexData[]     = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2};
    GLfloat positionData[] = {0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              -1.0f + halfPixel,
                              -1.0f,
                              -1.0f + halfPixel,
                              1.0f};

    glVertexAttribIPointer(mIntAttribLocation, 1, GL_INT, 0, vertexData);

    GLint positionLocation = glGetAttribLocation(mProgram, "position");
    glEnableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, positionData);

    glUseProgram(mProgram);
    glDrawArrays(GL_LINES, 10, 2);

    GLint pixelValue[4] = {0};
    glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_INT, &pixelValue);

    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(vertexData[11], pixelValue[0]);
}

// Test drawing a simple triangle strip with flat shading, and different valued vertices.
TEST_P(ProvokingVertexTest, FlatTriStrip)
{
    GLint vertexData[]     = {1, 2, 3, 4, 5, 6};
    GLfloat positionData[] = {-1.0f, -1.0f, -1.0f, 1.0f,  0.0f, -1.0f,
                              0.0f,  1.0f,  1.0f,  -1.0f, 1.0f, 1.0f};

    glVertexAttribIPointer(mIntAttribLocation, 1, GL_INT, 0, vertexData);

    GLint positionLocation = glGetAttribLocation(mProgram, "position");
    glEnableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, positionData);

    glUseProgram(mProgram);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);

    std::vector<GLint> pixelBuffer(getWindowWidth() * getWindowHeight() * 4, 0);
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA_INTEGER, GL_INT,
                 &pixelBuffer[0]);

    ASSERT_GL_NO_ERROR();

    for (unsigned int triIndex = 0; triIndex < 4; ++triIndex)
    {
        GLfloat sumX = positionData[triIndex * 2 + 0] + positionData[triIndex * 2 + 2] +
                       positionData[triIndex * 2 + 4];
        GLfloat sumY = positionData[triIndex * 2 + 1] + positionData[triIndex * 2 + 3] +
                       positionData[triIndex * 2 + 5];

        float centerX = sumX / 3.0f * 0.5f + 0.5f;
        float centerY = sumY / 3.0f * 0.5f + 0.5f;
        unsigned int pixelX =
            static_cast<unsigned int>(centerX * static_cast<GLfloat>(getWindowWidth()));
        unsigned int pixelY =
            static_cast<unsigned int>(centerY * static_cast<GLfloat>(getWindowHeight()));
        unsigned int pixelIndex = pixelY * getWindowWidth() + pixelX;

        unsigned int provokingVertexIndex = triIndex + 2;

        EXPECT_EQ(vertexData[provokingVertexIndex], pixelBuffer[pixelIndex * 4]);
    }
}

// Test drawing an indexed triangle strip with flat shading and primitive restart.
TEST_P(ProvokingVertexTest, FlatTriStripPrimitiveRestart)
{
    // TODO(jmadill): Implement on the D3D back-end.
    ANGLE_SKIP_TEST_IF(IsD3D11());

    GLint indexData[]      = {0, 1, 2, -1, 1, 2, 3, 4, -1, 3, 4, 5};
    GLint vertexData[]     = {1, 2, 3, 4, 5, 6};
    GLfloat positionData[] = {-1.0f, -1.0f, -1.0f, 1.0f,  0.0f, -1.0f,
                              0.0f,  1.0f,  1.0f,  -1.0f, 1.0f, 1.0f};

    glVertexAttribIPointer(mIntAttribLocation, 1, GL_INT, 0, vertexData);

    GLint positionLocation = glGetAttribLocation(mProgram, "position");
    glEnableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, positionData);

    glDisable(GL_CULL_FACE);
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glUseProgram(mProgram);
    glDrawElements(GL_TRIANGLE_STRIP, 12, GL_UNSIGNED_INT, indexData);

    std::vector<GLint> pixelBuffer(getWindowWidth() * getWindowHeight() * 4, 0);
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA_INTEGER, GL_INT,
                 &pixelBuffer[0]);

    ASSERT_GL_NO_ERROR();

    // Account for primitive restart when checking the tris.
    GLint triOffsets[] = {0, 4, 5, 9};

    for (unsigned int triIndex = 0; triIndex < 4; ++triIndex)
    {
        GLint vertexA = indexData[triOffsets[triIndex] + 0];
        GLint vertexB = indexData[triOffsets[triIndex] + 1];
        GLint vertexC = indexData[triOffsets[triIndex] + 2];

        GLfloat sumX =
            positionData[vertexA * 2] + positionData[vertexB * 2] + positionData[vertexC * 2];
        GLfloat sumY = positionData[vertexA * 2 + 1] + positionData[vertexB * 2 + 1] +
                       positionData[vertexC * 2 + 1];

        float centerX = sumX / 3.0f * 0.5f + 0.5f;
        float centerY = sumY / 3.0f * 0.5f + 0.5f;
        unsigned int pixelX =
            static_cast<unsigned int>(centerX * static_cast<GLfloat>(getWindowWidth()));
        unsigned int pixelY =
            static_cast<unsigned int>(centerY * static_cast<GLfloat>(getWindowHeight()));
        unsigned int pixelIndex = pixelY * getWindowWidth() + pixelX;

        unsigned int provokingVertexIndex = triIndex + 2;

        EXPECT_EQ(vertexData[provokingVertexIndex], pixelBuffer[pixelIndex * 4]);
    }
}

// Test with FRONT_CONVENTION if we have ANGLE_provoking_vertex.
TEST_P(ProvokingVertexTest, ANGLEProvokingVertex)
{
    int32_t vertexData[] = {1, 2, 3};
    float positionData[] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};

    glEnableVertexAttribArray(mIntAttribLocation);
    glVertexAttribIPointer(mIntAttribLocation, 1, GL_INT, 0, vertexData);

    GLint positionLocation = glGetAttribLocation(mProgram, "position");
    glEnableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, positionData);

    glUseProgram(mProgram);
    ASSERT_GL_NO_ERROR();

    const auto &fnExpectId = [&](int id) {
        const int32_t zero[4] = {};
        glClearBufferiv(GL_COLOR, 0, zero);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        int32_t pixelValue[4] = {0};
        glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_INT, &pixelValue);

        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(vertexData[id], pixelValue[0]);
    };

    fnExpectId(2);

    const bool hasExt = IsGLExtensionEnabled("GL_ANGLE_provoking_vertex");
    if (IsD3D11())
    {
        EXPECT_TRUE(hasExt);
    }
    if (hasExt)
    {
        GLint mode;
        glGetIntegerv(GL_PROVOKING_VERTEX, &mode);
        EXPECT_EQ(mode, GL_LAST_VERTEX_CONVENTION);

        glProvokingVertexANGLE(GL_FIRST_VERTEX_CONVENTION);
        glGetIntegerv(GL_PROVOKING_VERTEX, &mode);
        EXPECT_EQ(mode, GL_FIRST_VERTEX_CONVENTION);

        fnExpectId(0);
    }
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ProvokingVertexTest);
ANGLE_INSTANTIATE_TEST(ProvokingVertexTest, ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES(), ES3_METAL());

}  // anonymous namespace
