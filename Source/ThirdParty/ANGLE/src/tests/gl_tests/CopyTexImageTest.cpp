//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

namespace angle
{

class CopyTexImageTest : public ANGLETest
{
  protected:
    CopyTexImageTest()
    {
        setWindowWidth(32);
        setWindowHeight(32);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testSetUp() override
    {
        mTextureProgram =
            CompileProgram(essl1_shaders::vs::Texture2D(), essl1_shaders::fs::Texture2D());
        if (mTextureProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        mTextureUniformLocation =
            glGetUniformLocation(mTextureProgram, essl1_shaders::Texture2DUniform());

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override { glDeleteProgram(mTextureProgram); }

    void initializeResources(GLenum internalFormat, GLenum format, GLenum type)
    {
        for (size_t i = 0; i < kFboCount; ++i)
        {
            glBindTexture(GL_TEXTURE_2D, mFboTextures[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, kFboSizes[i], kFboSizes[i], 0, format,
                         type, nullptr);

            // Disable mipmapping
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            glBindFramebuffer(GL_FRAMEBUFFER, mFbos[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   mFboTextures[i], 0);

            glClearColor(kFboColors[i][0], kFboColors[i][1], kFboColors[i][2], kFboColors[i][3]);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        ASSERT_GL_NO_ERROR();
    }

    void initializeResources(GLenum format, GLenum type)
    {
        initializeResources(format, format, type);
    }

    void verifyResults(GLuint texture,
                       const GLubyte data[4],
                       GLint fboSize,
                       GLint xs,
                       GLint ys,
                       GLint xe,
                       GLint ye)
    {
        glViewport(0, 0, fboSize, fboSize);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Draw a quad with the target texture
        glUseProgram(mTextureProgram);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(mTextureUniformLocation, 0);

        drawQuad(mTextureProgram, essl1_shaders::PositionAttrib(), 0.5f);

        // Expect that the rendered quad has the same color as the source texture
        EXPECT_PIXEL_NEAR(xs, ys, data[0], data[1], data[2], data[3], 1.0);
        EXPECT_PIXEL_NEAR(xs, ye - 1, data[0], data[1], data[2], data[3], 1.0);
        EXPECT_PIXEL_NEAR(xe - 1, ys, data[0], data[1], data[2], data[3], 1.0);
        EXPECT_PIXEL_NEAR(xe - 1, ye - 1, data[0], data[1], data[2], data[3], 1.0);
        EXPECT_PIXEL_NEAR((xs + xe) / 2, (ys + ye) / 2, data[0], data[1], data[2], data[3], 1.0);
    }

    void verifyCheckeredResults(GLuint texture,
                                const GLubyte data0[4],
                                const GLubyte data1[4],
                                const GLubyte data2[4],
                                const GLubyte data3[4],
                                GLint fboWidth,
                                GLint fboHeight)
    {
        glViewport(0, 0, fboWidth, fboHeight);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Draw a quad with the target texture
        glUseProgram(mTextureProgram);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(mTextureUniformLocation, 0);

        drawQuad(mTextureProgram, essl1_shaders::PositionAttrib(), 0.5f);

        // Expect that the rendered quad has the same color as the source texture
        EXPECT_PIXEL_EQ(fboWidth / 4, fboHeight / 4, data0[0], data0[1], data0[2], data0[3]);
        EXPECT_PIXEL_EQ(fboWidth / 4, 3 * fboHeight / 4, data1[0], data1[1], data1[2], data1[3]);
        EXPECT_PIXEL_EQ(3 * fboWidth / 4, fboHeight / 4, data2[0], data2[1], data2[2], data2[3]);
        EXPECT_PIXEL_EQ(3 * fboWidth / 4, 3 * fboHeight / 4, data3[0], data3[1], data3[2],
                        data3[3]);
    }

    void runCopyTexImageTest(GLenum format, GLubyte expected[3][4])
    {
        GLTexture tex;
        glBindTexture(GL_TEXTURE_2D, tex);

        // Disable mipmapping
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // Perform the copy multiple times.
        //
        // - The first time, a new texture is created
        // - The second time, as the fbo size is the same as previous, the texture storage is not
        //   recreated.
        // - The third time, the fbo size is different, so a new texture is created.
        for (size_t i = 0; i < kFboCount; ++i)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, mFbos[i]);

            glCopyTexImage2D(GL_TEXTURE_2D, 0, format, 0, 0, kFboSizes[i], kFboSizes[i], 0);
            ASSERT_GL_NO_ERROR();

            verifyResults(tex, expected[i], kFboSizes[i], 0, 0, kFboSizes[i], kFboSizes[i]);
        }
    }

    // x, y, width, height specify the portion of fbo to be copied into tex.
    // flip_y specifies if the glCopyTextImage must be done from y-flipped fbo.
    void runCopyTexImageTestCheckered(GLenum format,
                                      const uint32_t x[3],
                                      const uint32_t y[3],
                                      const uint32_t width[3],
                                      const uint32_t height[3],
                                      const GLubyte expectedData0[4],
                                      const GLubyte expectedData1[4],
                                      const GLubyte expectedData2[4],
                                      const GLubyte expectedData3[4],
                                      bool mesaFlipY)
    {
        GLTexture tex;
        glBindTexture(GL_TEXTURE_2D, tex);

        // Disable mipmapping
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // Perform the copy multiple times.
        for (size_t i = 0; i < kFboCount; ++i)
        {
            glViewport(0, 0, kFboSizes[i], kFboSizes[i]);
            glBindFramebuffer(GL_FRAMEBUFFER, mFbos[i]);

            ANGLE_GL_PROGRAM(checkerProgram, essl1_shaders::vs::Passthrough(),
                             essl1_shaders::fs::Checkered());
            drawQuad(checkerProgram.get(), essl1_shaders::PositionAttrib(), 0.5f);
            EXPECT_GL_NO_ERROR();

            if (mesaFlipY)
                glFramebufferParameteriMESA(GL_FRAMEBUFFER, GL_FRAMEBUFFER_FLIP_Y_MESA, 1);
            glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x[i], y[i], width[i], height[i], 0);
            ASSERT_GL_NO_ERROR();

            verifyCheckeredResults(tex, expectedData0, expectedData1, expectedData2, expectedData3,
                                   kFboSizes[i], kFboSizes[i]);
        }
    }

    void runCopyTexSubImageTest(GLenum format, GLubyte expected[3][4])
    {
        GLTexture tex;
        glBindTexture(GL_TEXTURE_2D, tex);

        // Disable mipmapping
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // Create the texture with copy of the first fbo.
        glBindFramebuffer(GL_FRAMEBUFFER, mFbos[0]);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, format, 0, 0, kFboSizes[0], kFboSizes[0], 0);
        ASSERT_GL_NO_ERROR();

        verifyResults(tex, expected[0], kFboSizes[0], 0, 0, kFboSizes[0], kFboSizes[0]);

        // Make sure out-of-bound writes to the texture return invalid value.
        glBindFramebuffer(GL_FRAMEBUFFER, mFbos[1]);

        // xoffset < 0 and yoffset < 0
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, -1, -1, 0, 0, kFboSizes[0], kFboSizes[0]);
        ASSERT_GL_ERROR(GL_INVALID_VALUE);

        // xoffset + width > w and yoffset + height > h
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 0, 0, kFboSizes[0], kFboSizes[0]);
        ASSERT_GL_ERROR(GL_INVALID_VALUE);

        // xoffset + width > w and yoffset + height > h, out of bounds
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, -1, -1, 1 + kFboSizes[0], 1 + kFboSizes[0]);
        ASSERT_GL_ERROR(GL_INVALID_VALUE);

        // Copy the second fbo over a portion of the image.
        GLint offset = kFboSizes[0] / 2;
        GLint extent = kFboSizes[0] - offset;

        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, offset, offset, kFboSizes[1] / 2, kFboSizes[1] / 2,
                            extent, extent);
        ASSERT_GL_NO_ERROR();

        verifyResults(tex, expected[1], kFboSizes[0], offset, offset, kFboSizes[0], kFboSizes[0]);

        // The rest of the image should be untouched
        verifyResults(tex, expected[0], kFboSizes[0], 0, 0, offset, offset);
        verifyResults(tex, expected[0], kFboSizes[0], offset, 0, kFboSizes[0], offset);
        verifyResults(tex, expected[0], kFboSizes[0], 0, offset, offset, kFboSizes[0]);

        // Copy the third fbo over another portion of the image.
        glBindFramebuffer(GL_FRAMEBUFFER, mFbos[2]);

        offset = kFboSizes[0] / 4;
        extent = kFboSizes[0] - offset;

        // While width and height are set as 3/4 of the size, the fbo offset is given such that
        // after clipping, width and height are effectively 1/2 of the size.
        GLint srcOffset       = kFboSizes[2] - kFboSizes[0] / 2;
        GLint effectiveExtent = kFboSizes[0] / 2;

        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, offset, offset, srcOffset, srcOffset, extent, extent);
        ASSERT_GL_NO_ERROR();

        verifyResults(tex, expected[2], kFboSizes[0], offset, offset, effectiveExtent,
                      effectiveExtent);

        // The rest of the image should be untouched
        verifyResults(tex, expected[1], kFboSizes[0], offset + effectiveExtent, kFboSizes[0] / 2,
                      kFboSizes[0], kFboSizes[0]);
        verifyResults(tex, expected[1], kFboSizes[0], kFboSizes[0] / 2, offset + effectiveExtent,
                      kFboSizes[0], kFboSizes[0]);

        verifyResults(tex, expected[0], kFboSizes[0], 0, 0, kFboSizes[0], offset);
        verifyResults(tex, expected[0], kFboSizes[0], 0, 0, offset, kFboSizes[0]);
        verifyResults(tex, expected[0], kFboSizes[0], offset + effectiveExtent, 0, kFboSizes[0],
                      kFboSizes[0] / 2);
        verifyResults(tex, expected[0], kFboSizes[0], 0, offset + effectiveExtent, kFboSizes[0] / 2,
                      kFboSizes[0]);
    }

    GLuint mTextureProgram;
    GLint mTextureUniformLocation;

    static constexpr uint32_t kFboCount = 3;
    GLFramebuffer mFbos[kFboCount];
    GLTexture mFboTextures[kFboCount];

    static constexpr uint32_t kFboSizes[kFboCount]    = {16, 16, 32};
    static constexpr GLfloat kFboColors[kFboCount][4] = {{0.25f, 1.0f, 0.75f, 0.5f},
                                                         {1.0f, 0.75f, 0.5f, 0.25f},
                                                         {0.5f, 0.25f, 1.0f, 0.75f}};
};

// Until C++17, need to redundantly declare the constexpr members outside the class (only the
// arrays, because the others are already const-propagated and not needed by the linker).
constexpr uint32_t CopyTexImageTest::kFboSizes[];
constexpr GLfloat CopyTexImageTest::kFboColors[][4];

TEST_P(CopyTexImageTest, RGBAToRGB)
{
    GLubyte expected[3][4] = {
        {64, 255, 191, 255},
        {255, 191, 127, 255},
        {127, 64, 255, 255},
    };

    initializeResources(GL_RGBA, GL_UNSIGNED_BYTE);
    runCopyTexImageTest(GL_RGB, expected);
}

TEST_P(CopyTexImageTest, RGBAToL)
{
    GLubyte expected[3][4] = {
        {64, 64, 64, 255},
        {255, 255, 255, 255},
        {127, 127, 127, 255},
    };

    initializeResources(GL_RGBA, GL_UNSIGNED_BYTE);
    runCopyTexImageTest(GL_LUMINANCE, expected);
}

TEST_P(CopyTexImageTest, RGBToL)
{
    GLubyte expected[3][4] = {
        {64, 64, 64, 255},
        {255, 255, 255, 255},
        {127, 127, 127, 255},
    };

    initializeResources(GL_RGB, GL_UNSIGNED_BYTE);
    runCopyTexImageTest(GL_LUMINANCE, expected);
}

TEST_P(CopyTexImageTest, RGBAToLA)
{
    GLubyte expected[3][4] = {
        {64, 64, 64, 127},
        {255, 255, 255, 64},
        {127, 127, 127, 191},
    };

    initializeResources(GL_RGBA, GL_UNSIGNED_BYTE);
    runCopyTexImageTest(GL_LUMINANCE_ALPHA, expected);
}

TEST_P(CopyTexImageTest, RGBAToA)
{
    GLubyte expected[3][4] = {
        {0, 0, 0, 127},
        {0, 0, 0, 64},
        {0, 0, 0, 191},
    };

    initializeResources(GL_RGBA, GL_UNSIGNED_BYTE);
    runCopyTexImageTest(GL_ALPHA, expected);
}

TEST_P(CopyTexImageTest, SubImageRGBAToRGB)
{
    GLubyte expected[3][4] = {
        {64, 255, 191, 255},
        {255, 191, 127, 255},
        {127, 64, 255, 255},
    };

    initializeResources(GL_RGBA, GL_UNSIGNED_BYTE);
    runCopyTexSubImageTest(GL_RGB, expected);
}

TEST_P(CopyTexImageTest, SubImageRGBAToL)
{
    GLubyte expected[3][4] = {
        {64, 64, 64, 255},
        {255, 255, 255, 255},
        {127, 127, 127, 255},
    };

    initializeResources(GL_RGBA, GL_UNSIGNED_BYTE);
    runCopyTexSubImageTest(GL_LUMINANCE, expected);
}

TEST_P(CopyTexImageTest, SubImageRGBAToLA)
{
    GLubyte expected[3][4] = {
        {64, 64, 64, 127},
        {255, 255, 255, 64},
        {127, 127, 127, 191},
    };

    initializeResources(GL_RGBA, GL_UNSIGNED_BYTE);
    runCopyTexSubImageTest(GL_LUMINANCE_ALPHA, expected);
}

TEST_P(CopyTexImageTest, SubImageRGBToL)
{
    GLubyte expected[3][4] = {
        {64, 64, 64, 255},
        {255, 255, 255, 255},
        {127, 127, 127, 255},
    };

    initializeResources(GL_RGB, GL_UNSIGNED_BYTE);
    runCopyTexSubImageTest(GL_LUMINANCE, expected);
}

TEST_P(CopyTexImageTest, RGBXToL)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_rgbx_internal_format"));

    GLubyte expected[3][4] = {
        {64, 64, 64, 255},
        {255, 255, 255, 255},
        {127, 127, 127, 255},
    };

    initializeResources(GL_RGBX8_ANGLE, GL_RGB, GL_UNSIGNED_BYTE);
    runCopyTexImageTest(GL_LUMINANCE, expected);
}

// Read default framebuffer with glCopyTexImage2D().
TEST_P(CopyTexImageTest, DefaultFramebuffer)
{
    // Seems to be a bug in Mesa with the GLX back end: cannot read framebuffer until we draw to it.
    // glCopyTexImage2D() below will fail without this clear.
    glClear(GL_COLOR_BUFFER_BIT);

    const GLint w = getWindowWidth(), h = getWindowHeight();
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, w, h, 0);
    EXPECT_GL_NO_ERROR();
}

// Read default framebuffer with glCopyTexSubImage2D().
TEST_P(CopyTexImageTest, SubDefaultFramebuffer)
{
    // Seems to be a bug in Mesa with the GLX back end: cannot read framebuffer until we draw to it.
    // glCopyTexSubImage2D() below will fail without this clear.
    glClear(GL_COLOR_BUFFER_BIT);

    const GLint w = getWindowWidth(), h = getWindowHeight();
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
    EXPECT_GL_NO_ERROR();
}

// Calling CopyTexSubImage from cubeMap texture.
TEST_P(CopyTexImageTest, CopyTexSubImageFromCubeMap)
{
    constexpr GLsizei kCubeMapFaceCount = 6;

    // The framebuffer will be a face of a cube map with a different colors for each face.  Each
    // glCopyTexSubImage2D will take one face of this image to copy over a pixel in a 1x6
    // framebuffer.
    GLColor fboPixels[kCubeMapFaceCount]   = {GLColor::red,  GLColor::yellow, GLColor::green,
                                            GLColor::cyan, GLColor::blue,   GLColor::magenta};
    GLColor whitePixels[kCubeMapFaceCount] = {GLColor::white, GLColor::white, GLColor::white,
                                              GLColor::white, GLColor::white, GLColor::white};

    GLTexture fboTex;
    glBindTexture(GL_TEXTURE_CUBE_MAP, fboTex);
    for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
         face++)
    {
        GLsizei faceIndex = face - GL_TEXTURE_CUBE_MAP_POSITIVE_X;

        glTexImage2D(face, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &fboPixels[faceIndex]);
    }

    GLTexture dstTex;
    glBindTexture(GL_TEXTURE_2D, dstTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kCubeMapFaceCount, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 whitePixels);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
         face++)
    {
        GLsizei faceIndex = face - GL_TEXTURE_CUBE_MAP_POSITIVE_X;

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, face, fboTex, 0);

        ASSERT_GL_NO_ERROR();
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

        // Copy the fbo (a cube map face) into a pixel of the destination texture.
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, faceIndex, 0, 0, 0, 1, 1);
    }

    // Make sure all the copies are done correctly.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);

    ASSERT_GL_NO_ERROR();
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    for (GLsizei faceIndex = 0; faceIndex < kCubeMapFaceCount; ++faceIndex)
    {
        EXPECT_PIXEL_COLOR_EQ(faceIndex, 0, fboPixels[faceIndex]);
    }
}

// Calling CopyTexSubImage to a non-cube-complete texture.
TEST_P(CopyTexImageTest, CopyTexSubImageToNonCubeCompleteDestination)
{
    constexpr GLsizei kCubeMapFaceCount = 6;

    // The framebuffer will be a 1x6 image with 6 different colors.  Each glCopyTexSubImage2D will
    // take one pixel of this image to copy over each face of a cube map.
    GLColor fboPixels[kCubeMapFaceCount] = {GLColor::red,  GLColor::yellow, GLColor::green,
                                            GLColor::cyan, GLColor::blue,   GLColor::magenta};
    GLColor whitePixel                   = GLColor::white;

    GLTexture fboTex;
    glBindTexture(GL_TEXTURE_2D, fboTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kCubeMapFaceCount, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 fboPixels);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);

    ASSERT_GL_NO_ERROR();
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    GLTexture cubeMap;
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);

    for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
         face++)
    {
        GLsizei faceIndex = face - GL_TEXTURE_CUBE_MAP_POSITIVE_X;

        // Initialize the face with a color not found in the fbo.
        glTexImage2D(face, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &whitePixel);

        // Copy one pixel from the fbo into this face.  The first 5 copies are done on a
        // non-cube-complete texture.
        glCopyTexSubImage2D(face, 0, 0, 0, faceIndex, 0, 1, 1);
    }

    // Make sure all the copies are done correctly.
    for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
         face++)
    {
        GLsizei faceIndex = face - GL_TEXTURE_CUBE_MAP_POSITIVE_X;

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, face, cubeMap, 0);

        ASSERT_GL_NO_ERROR();
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

        EXPECT_PIXEL_COLOR_EQ(0, 0, fboPixels[faceIndex]);
    }
}

// Deleting textures after copying to them. http://anglebug.com/4267
TEST_P(CopyTexImageTest, DeleteAfterCopyingToTextures)
{
    // TODO(anglebug.com/5360): Failing on ARM-based Apple DTKs.
    ANGLE_SKIP_TEST_IF(IsOSX() && IsARM64() && IsDesktopOpenGL());

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLTexture texture2;
    glBindTexture(GL_TEXTURE_2D, texture2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Perform CopyTexImage2D
    glBindTexture(GL_TEXTURE_2D, texture);
    glCopyTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 0, 0, 2, 2, 0);
    ASSERT_GL_NO_ERROR();
    // Not necessary to do any CopyTexImage2D operations to texture2.

    // Perform CopyTexSubImage2D
    glBindTexture(GL_TEXTURE_2D, texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 0, 0, 1, 1);
    ASSERT_GL_NO_ERROR();
    glBindTexture(GL_TEXTURE_2D, texture2);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
    ASSERT_GL_NO_ERROR();

    // Clean up - provokes crash on buggy drivers.
    texture.reset();
    // Crashes on Intel GPUs on macOS.
    texture2.reset();
}
// Test if glCopyTexImage2D() implementation performs conversions well from GL_TEXTURE_3D to
// GL_TEXTURE_2D.
// This is similar to CopyTexImageTestES3.CopyTexSubImageFromTexture3D but for GL_OES_texture_3D
// extension.
TEST_P(CopyTexImageTest, CopyTexSubImageFrom3DTexureOES)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_3D"));
    // TODO(anglebug.com/3801)
    // Seems to fail on D3D11 Windows.
    ANGLE_SKIP_TEST_IF(IsD3D11() && IsWindows());

    // http://anglebug.com/4927
    ANGLE_SKIP_TEST_IF((IsPixel2() || IsNexus5X()) && IsOpenGLES());

    constexpr GLsizei kDepth = 6;

    // The framebuffer will be a slice of a 3d texture with a different colors for each slice.  Each
    // glCopyTexSubImage2D will take one face of this image to copy over a pixel in a 1x6
    // framebuffer.
    GLColor fboPixels[kDepth]   = {GLColor::red,  GLColor::yellow, GLColor::green,
                                 GLColor::cyan, GLColor::blue,   GLColor::magenta};
    GLColor whitePixels[kDepth] = {GLColor::white, GLColor::white, GLColor::white,
                                   GLColor::white, GLColor::white, GLColor::white};

    GLTexture fboTex;
    glBindTexture(GL_TEXTURE_3D, fboTex);
    glTexImage3DOES(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, kDepth, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    fboPixels);

    GLTexture dstTex;
    glBindTexture(GL_TEXTURE_2D, dstTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kDepth, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixels);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    for (GLsizei slice = 0; slice < kDepth; ++slice)
    {
        glFramebufferTexture3DOES(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, fboTex, 0,
                                  slice);

        ASSERT_GL_NO_ERROR();
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

        // Copy the fbo (a 3d slice) into a pixel of the destination texture.
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, slice, 0, 0, 0, 1, 1);
    }

    // Make sure all the copies are done correctly.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);

    ASSERT_GL_NO_ERROR();
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    for (GLsizei slice = 0; slice < kDepth; ++slice)
    {
        EXPECT_PIXEL_COLOR_EQ(slice, 0, fboPixels[slice]);
    }
}

// Tests image copy from y-flipped fbo works.
TEST_P(CopyTexImageTest, CopyTexImageMesaYFlip)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_MESA_framebuffer_flip_y"));

    std::array<uint32_t, 3> copyOrigin{0};
    std::array<uint32_t, 3> copySize;
    for (size_t i = 0; i < kFboCount; i++)
        copySize[i] = kFboSizes[i];

    initializeResources(GL_RGBA, GL_UNSIGNED_BYTE);
    runCopyTexImageTestCheckered(GL_RGBA, copyOrigin.data(), copyOrigin.data(), copySize.data(),
                                 copySize.data(), GLColor::green.data(), GLColor::red.data(),
                                 GLColor::yellow.data(), GLColor::blue.data(),
                                 true /* mesaFlipY */);
}

// Tests image partial copy from y-flipped fbo works.
TEST_P(CopyTexImageTest, CopyTexImageMesaYFlipPartial)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_MESA_framebuffer_flip_y"));

    std::array<uint32_t, kFboCount> copyX;
    std::array<uint32_t, kFboCount> copyY{0};
    std::array<uint32_t, kFboCount> copyWidth;
    std::array<uint32_t, kFboCount> copyHeight;

    for (size_t i = 0; i < kFboCount; i++)
    {
        copyX[i]      = kFboSizes[i] / 2;
        copyHeight[i] = kFboSizes[i];
    }
    copyWidth = copyX;

    initializeResources(GL_RGBA, GL_UNSIGNED_BYTE);
    runCopyTexImageTestCheckered(GL_RGBA, copyX.data(), copyY.data(), copyWidth.data(),
                                 copyHeight.data(), GLColor::yellow.data(), GLColor::blue.data(),
                                 GLColor::yellow.data(), GLColor::blue.data(),
                                 true /* mesaFlipY */);
}

// Tests subimage copy from y-flipped fbo works.
TEST_P(CopyTexImageTest, CopyTexSubImageMesaYFlip)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_MESA_framebuffer_flip_y"));

    GLuint format = GL_RGBA;
    initializeResources(format, GL_UNSIGNED_BYTE);

    glViewport(0, 0, kFboSizes[0], kFboSizes[0]);
    glBindFramebuffer(GL_FRAMEBUFFER, mFbos[0]);

    ANGLE_GL_PROGRAM(checkerProgram, essl1_shaders::vs::Passthrough(),
                     essl1_shaders::fs::Checkered());
    drawQuad(checkerProgram.get(), essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_NO_ERROR();

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);

    // Disable mipmapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Create the texture with copy of the first fbo.
    glBindFramebuffer(GL_FRAMEBUFFER, mFbos[0]);
    glFramebufferParameteriMESA(GL_FRAMEBUFFER, GL_FRAMEBUFFER_FLIP_Y_MESA, 1);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, format, 0, 0, kFboSizes[0], kFboSizes[0], 0);
    ASSERT_GL_NO_ERROR();

    // Make sure out-of-bound writes to the texture return invalid value.
    glBindFramebuffer(GL_FRAMEBUFFER, mFbos[1]);
    drawQuad(checkerProgram.get(), essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_NO_ERROR();

    glFramebufferParameteriMESA(GL_FRAMEBUFFER, GL_FRAMEBUFFER_FLIP_Y_MESA, 1);

    // xoffset < 0 and yoffset < 0
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, -1, -1, 0, 0, kFboSizes[0], kFboSizes[0]);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    // xoffset + width > w and yoffset + height > h
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 0, 0, kFboSizes[0], kFboSizes[0]);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    // xoffset + width > w and yoffset + height > h, out of bounds
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, -1, -1, 1 + kFboSizes[0], 1 + kFboSizes[0]);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    // Copy the second fbo over a portion of the image.
    GLint offset = kFboSizes[0] / 2;
    GLint extent = kFboSizes[0] - offset;
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, offset, offset, kFboSizes[1] / 2, kFboSizes[1] / 2,
                        extent, extent);
    ASSERT_GL_NO_ERROR();

    // Only part of the image is changed.
    verifyCheckeredResults(tex, GLColor::green.data(), GLColor::red.data(), GLColor::yellow.data(),
                           GLColor::blue.data(), kFboSizes[0], kFboSizes[0]);
}

// specialization of CopyTexImageTest is added so that some tests can be explicitly run with an ES3
// context
class CopyTexImageTestES3 : public CopyTexImageTest
{
  protected:
    void initialize3DTexture(GLTexture &texture,
                             const GLsizei imageWidth,
                             const GLsizei imageHeight,
                             const GLsizei imageDepth,
                             const GLColor *textureData);
    void initialize2DTexture(GLTexture &texture,
                             const GLsizei imageWidth,
                             const GLsizei imageHeight,
                             const GLColor *textureData);
    void initialize2DTextureUShort4444(GLTexture &texture,
                                       const GLsizei imageWidth,
                                       const GLsizei imageHeight,
                                       const GLColor *textureData);
    void fillTexture(std::vector<GLColor> &texture, const GLColor color);
    void clearTexture(GLFramebuffer &fbo, GLTexture &texture, const GLColor color);
    void copyTexSubImage3D(GLTexture &subTexture2D,
                           const GLint xOffset,
                           const GLint yOffset,
                           const GLsizei subImageWidth,
                           const GLsizei subImageHeight,
                           const GLsizei imageDepth);
    void verifyCopyTexSubImage3D(GLTexture &texture3D,
                                 const GLint xOffset,
                                 const GLint yOffset,
                                 const GLColor subImageColor);

    // Constants
    const GLColor kSubImageColor = GLColor::yellow;
    // 3D image dimensions
    const GLsizei kImageWidth  = getWindowWidth();
    const GLsizei kImageHeight = getWindowHeight();
    const GLsizei kImageDepth  = 4;
    // 2D sub-image dimensions
    const GLsizei kSubImageWidth  = getWindowWidth() / 4;
    const GLsizei kSubImageHeight = getWindowHeight() / 4;
    // Sub-Image Offsets
    const GLint kXOffset = getWindowWidth() - kSubImageWidth;
    const GLint kYOffset = getWindowHeight() - kSubImageHeight;
};

//  The test verifies that glCopyTexSubImage2D generates a GL_INVALID_OPERATION error
//  when the read buffer is GL_NONE.
//  Reference: GLES 3.0.4, Section 3.8.5 Alternate Texture Image Specification Commands
TEST_P(CopyTexImageTestES3, ReadBufferIsNone)
{
    initializeResources(GL_RGBA, GL_UNSIGNED_BYTE);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, mFbos[0]);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, kFboSizes[0], kFboSizes[0], 0);

    glReadBuffer(GL_NONE);

    EXPECT_GL_NO_ERROR();
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 4, 4);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test CopyTexImage3D with some simple parameters with a 2D array texture.
TEST_P(CopyTexImageTestES3, 2DArraySubImage)
{
    // Seems to fail on AMD OpenGL Windows.
    ANGLE_SKIP_TEST_IF(IsAMD() && IsOpenGL() && IsWindows());

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);

    constexpr GLsizei kTexSize     = 4;
    constexpr GLsizei kLayerOffset = 1;
    constexpr GLsizei kLayers      = 2;

    // Clear screen to green.
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Initialize a two-layer 2D array texture with red.
    std::vector<GLColor> red(kTexSize * kTexSize * kLayers, GLColor::red);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, kTexSize, kTexSize, kLayers, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, red.data());
    glCopyTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, kLayerOffset, 0, 0, kTexSize, kTexSize);
    ASSERT_GL_NO_ERROR();

    // Check level 0 (red from image data) and 1 (green from backbuffer clear).
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 0);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 1);
    for (int x = 0; x < kTexSize; x++)
    {
        for (int y = 0; y < kTexSize; y++)
        {
            EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::green);
        }
    }
    ASSERT_GL_NO_ERROR();
}

// Test if glCopyTexImage2D() implementation performs conversions well from GL_TEXTURE_3D to
// GL_TEXTURE_2D.
TEST_P(CopyTexImageTestES3, CopyTexSubImageFromTexture3D)
{
    // TODO(anglebug.com/3801)
    // Seems to fail on D3D11 Windows.
    ANGLE_SKIP_TEST_IF(IsD3D11() && IsWindows());

    constexpr GLsizei kTexSize = 4;
    constexpr GLsizei kLayers  = 2;
    std::vector<GLColor> red(kTexSize * kTexSize * kLayers, GLColor::red);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindTexture(GL_TEXTURE_2D, 0);

    // We will be reading from zeroth color attachment.
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    GLTexture src_object_id;
    glBindTexture(GL_TEXTURE_3D, src_object_id);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, kTexSize, kTexSize, kLayers, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 1, kTexSize, kTexSize, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    red.data());
    glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, src_object_id, 0, 1);
    ASSERT_GL_NO_ERROR();

    GLTexture dst_object_id;
    glBindTexture(GL_TEXTURE_2D, dst_object_id);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, kTexSize, kTexSize, 0);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_object_id,
                           0);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    ASSERT_GL_NO_ERROR();
}

// Test that copying from a non-zero base texture works.
TEST_P(CopyTexImageTestES3, CopyTexSubImageFromNonZeroBase)
{
    // http://anglebug.com/5000
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsIntel() && IsWindows());

    constexpr GLsizei kTexSize = 4;
    std::vector<GLColor> red(kTexSize * kTexSize, GLColor::red);
    std::vector<GLColor> green(kTexSize * kTexSize, GLColor::green);

    // Create a framebuffer attached to a non-zero base texture
    GLTexture srcColor;
    glBindTexture(GL_TEXTURE_2D, srcColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kTexSize, kTexSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 red.data());
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kTexSize, kTexSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 green.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    ASSERT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srcColor, 1);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Create a texture with an identical format
    GLTexture dstColor;
    glBindTexture(GL_TEXTURE_2D, dstColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kTexSize, kTexSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Copy into a part of this texture.
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, kTexSize / 2, kTexSize / 2);
    ASSERT_GL_NO_ERROR();

    // Verify it.
    constexpr std::array<GLubyte, 4> kExpected = {0, 255, 0, 255};
    verifyResults(dstColor, kExpected.data(), kTexSize, 0, 0, kTexSize / 2, kTexSize / 2);

    // Copy into another part of the texture.  The previous verification ensures that the texture's
    // internal image is allocated, so this should be a direct copy.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, kTexSize / 2, kTexSize / 2, 0, 0, kTexSize / 2,
                        kTexSize / 2);
    ASSERT_GL_NO_ERROR();

    // Verify it.
    verifyResults(dstColor, kExpected.data(), kTexSize, kTexSize / 2, kTexSize / 2, kTexSize,
                  kTexSize);
}

// Test that copying into a non-zero base texture works.
TEST_P(CopyTexImageTestES3, CopyTexSubImageToNonZeroBase)
{
    // http://anglebug.com/5000
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsIntel() && IsWindows());

    constexpr GLsizei kTexSize = 4;
    std::vector<GLColor> green(kTexSize * kTexSize, GLColor::green);

    // Create a framebuffer attached to a non-zero base texture
    GLTexture srcColor;
    glBindTexture(GL_TEXTURE_2D, srcColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kTexSize, kTexSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 green.data());
    ASSERT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srcColor, 0);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Create a texture with an identical format
    GLTexture dstColor;
    glBindTexture(GL_TEXTURE_2D, dstColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kTexSize, kTexSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kTexSize, kTexSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Copy into a part of this texture.
    glCopyTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 0, 0, kTexSize / 2, kTexSize / 2);
    ASSERT_GL_NO_ERROR();

    // Verify it.
    constexpr std::array<GLubyte, 4> kExpected = {0, 255, 0, 255};
    verifyResults(dstColor, kExpected.data(), kTexSize, 0, 0, kTexSize / 2, kTexSize / 2);

    // Copy into another part of the texture.  The previous verification ensures that the texture's
    // internal image is allocated, so this should be a direct copy.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 1, kTexSize / 2, kTexSize / 2, 0, 0, kTexSize / 2,
                        kTexSize / 2);
    ASSERT_GL_NO_ERROR();

    // Verify it.
    verifyResults(dstColor, kExpected.data(), kTexSize, kTexSize / 2, kTexSize / 2, kTexSize,
                  kTexSize);
}

// Initialize the 3D texture we will copy the subImage data into
void CopyTexImageTestES3::initialize3DTexture(GLTexture &texture,
                                              const GLsizei imageWidth,
                                              const GLsizei imageHeight,
                                              const GLsizei imageDepth,
                                              const GLColor *textureData)
{
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, imageWidth, imageHeight, imageDepth, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, textureData);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void CopyTexImageTestES3::initialize2DTexture(GLTexture &texture,
                                              const GLsizei imageWidth,
                                              const GLsizei imageHeight,
                                              const GLColor *textureData)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 textureData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void CopyTexImageTestES3::initialize2DTextureUShort4444(GLTexture &texture,
                                                        const GLsizei imageWidth,
                                                        const GLsizei imageHeight,
                                                        const GLColor *textureData)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA,
                 GL_UNSIGNED_SHORT_4_4_4_4, textureData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void CopyTexImageTestES3::fillTexture(std::vector<GLColor> &texture, const GLColor color)
{
    for (auto &texel : texture)
    {
        texel = color;
    }
}

void CopyTexImageTestES3::clearTexture(GLFramebuffer &fbo, GLTexture &texture, const GLColor color)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    glClearColor(color.R, color.G, color.B, color.A);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, color);
}

void CopyTexImageTestES3::copyTexSubImage3D(GLTexture &subTexture2D,
                                            const GLint xOffset,
                                            const GLint yOffset,
                                            const GLsizei subImageWidth,
                                            const GLsizei subImageHeight,
                                            const GLsizei imageDepth)
{
    // Copy the 2D sub-image into the 3D texture
    for (int currLayer = 0; currLayer < imageDepth; ++currLayer)
    {
        // Bind the 2D texture to GL_COLOR_ATTACHMENT0
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               subTexture2D, 0);
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, xOffset, yOffset, currLayer, 0, 0, subImageWidth,
                            subImageHeight);
        ASSERT_GL_NO_ERROR();
    }
}

void CopyTexImageTestES3::verifyCopyTexSubImage3D(GLTexture &texture3D,
                                                  const GLint xOffset,
                                                  const GLint yOffset,
                                                  const GLColor subImageColor)
{
    // Bind to an FBO to check the copy was successful
    for (int currLayer = 0; currLayer < kImageDepth; ++currLayer)
    {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture3D, 0, currLayer);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(xOffset, yOffset, subImageColor);
    }
}

// Test glCopyTexSubImage3D with initialized texture data
TEST_P(CopyTexImageTestES3, 3DSubImageRawTextureData)
{
    // Texture data
    std::vector<GLColor> textureData(kImageWidth * kImageHeight * kImageDepth);

    // Fill the textures with color
    fillTexture(textureData, GLColor::red);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture texture3D;
    initialize3DTexture(texture3D, kImageWidth, kImageHeight, kImageDepth, textureData.data());

    // The 2D texture that will be the sub-image copied into the destination texture
    GLTexture subTexture2D;
    initialize2DTexture(subTexture2D, kSubImageWidth, kSubImageHeight, nullptr);
    clearTexture(fbo, subTexture2D, kSubImageColor);

    // Copy the 2D subimage into the 3D texture
    copyTexSubImage3D(subTexture2D, kXOffset, kYOffset, kSubImageWidth, kSubImageHeight,
                      kImageDepth);

    // Verify the color wasn't overwritten
    verifyCopyTexSubImage3D(texture3D, 0, 0, GLColor::red);
    // Verify the copy succeeded
    verifyCopyTexSubImage3D(texture3D, kXOffset, kYOffset, kSubImageColor);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_3D, 0);
}

// Test glCopyTexSubImage3D with initialized texture data that was drawn to
TEST_P(CopyTexImageTestES3, 3DSubImageDrawTextureData)
{
    // TODO(anglebug.com/3801)
    ANGLE_SKIP_TEST_IF(IsWindows() && IsD3D11());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // The 3D texture we will copy the sub-image into
    GLTexture texture3D;
    initialize3DTexture(texture3D, kImageWidth, kImageHeight, kImageDepth, nullptr);

    // Draw to each layer in the 3D texture
    for (int currLayer = 0; currLayer < kImageDepth; ++currLayer)
    {
        ANGLE_GL_PROGRAM(greenProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
        glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture3D, 0,
                                  currLayer);
        glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture3D, 0,
                                  currLayer);
        ASSERT_GL_NO_ERROR();
        glUseProgram(greenProgram);
        drawQuad(greenProgram.get(), std::string(essl1_shaders::PositionAttrib()), 0.0f);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    }

    // The 2D texture that will be the sub-image copied into the destination texture
    GLTexture subTexture2D;
    initialize2DTexture(subTexture2D, kSubImageWidth, kSubImageHeight, nullptr);
    clearTexture(fbo, subTexture2D, kSubImageColor);

    // Copy the 2D sub-image into the 3D texture
    copyTexSubImage3D(subTexture2D, kXOffset, kYOffset, kSubImageWidth, kSubImageHeight,
                      kImageDepth);

    // Verify the color wasn't overwritten
    verifyCopyTexSubImage3D(texture3D, 0, 0, GLColor::green);
    // Verify the copy succeeded
    verifyCopyTexSubImage3D(texture3D, kXOffset, kYOffset, kSubImageColor);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_3D, 0);
}

// Test glCopyTexSubImage3D with mismatched texture formats
TEST_P(CopyTexImageTestES3, 3DSubImageDrawMismatchedTextureTypes)
{
    // TODO(anglebug.com/3801)
    ANGLE_SKIP_TEST_IF(IsWindows() && IsD3D11());

    // TODO(anglebug.com/5491)
    ANGLE_SKIP_TEST_IF(IsIOS() && IsOpenGLES());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // The 3D texture we will copy the sub-image into
    GLTexture texture3D;
    initialize3DTexture(texture3D, kImageWidth, kImageHeight, kImageDepth, nullptr);

    // Draw to each layer in the 3D texture
    for (int currLayer = 0; currLayer < kImageDepth; ++currLayer)
    {
        ANGLE_GL_PROGRAM(greenProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture3D, 0, currLayer);
        ASSERT_GL_NO_ERROR();
        glUseProgram(greenProgram);
        drawQuad(greenProgram.get(), std::string(essl1_shaders::PositionAttrib()), 0.0f);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    }

    // The 2D texture that will be the sub-image copied into the destination texture
    GLTexture subTexture2D;
    initialize2DTextureUShort4444(subTexture2D, kSubImageWidth, kSubImageHeight, nullptr);
    clearTexture(fbo, subTexture2D, kSubImageColor);

    // Copy the 2D sub-image into the 3D texture
    copyTexSubImage3D(subTexture2D, kXOffset, kYOffset, kSubImageWidth, kSubImageHeight,
                      kImageDepth);

    // Verify the color wasn't overwritten
    verifyCopyTexSubImage3D(texture3D, 0, 0, GLColor::green);
    // Verify the copy succeeded
    verifyCopyTexSubImage3D(texture3D, kXOffset, kYOffset, kSubImageColor);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_3D, 0);
}

ANGLE_INSTANTIATE_TEST(CopyTexImageTest,
                       ANGLE_ALL_TEST_PLATFORMS_ES2,
                       ES2_D3D11_PRESENT_PATH_FAST(),
                       ES3_VULKAN(),
                       WithEmulateCopyTexImage2DFromRenderbuffers(ES2_OPENGL()),
                       WithEmulateCopyTexImage2DFromRenderbuffers(ES2_OPENGLES()));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CopyTexImageTestES3);
ANGLE_INSTANTIATE_TEST(CopyTexImageTestES3,
                       ANGLE_ALL_TEST_PLATFORMS_ES3,
                       WithEmulateCopyTexImage2DFromRenderbuffers(ES3_OPENGL()),
                       WithEmulateCopyTexImage2DFromRenderbuffers(ES3_OPENGLES()));
}  // namespace angle
