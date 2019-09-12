//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Framebuffer tests:
//   Various tests related for Frambuffers.
//

#include "platform/WorkaroundsD3D.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

void ExpectFramebufferCompleteOrUnsupported(GLenum binding)
{
    GLenum status = glCheckFramebufferStatus(binding);
    EXPECT_TRUE(status == GL_FRAMEBUFFER_COMPLETE || status == GL_FRAMEBUFFER_UNSUPPORTED);
}

}  // anonymous namespace

class FramebufferFormatsTest : public ANGLETest
{
  protected:
    FramebufferFormatsTest() : mFramebuffer(0), mTexture(0), mRenderbuffer(0), mProgram(0)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void checkBitCount(GLuint fbo, GLenum channel, GLint minBits)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        GLint bits = 0;
        glGetIntegerv(channel, &bits);

        if (minBits == 0)
        {
            EXPECT_EQ(minBits, bits);
        }
        else
        {
            EXPECT_GE(bits, minBits);
        }
    }

    void testBitCounts(GLuint fbo,
                       GLint minRedBits,
                       GLint minGreenBits,
                       GLint minBlueBits,
                       GLint minAlphaBits,
                       GLint minDepthBits,
                       GLint minStencilBits)
    {
        checkBitCount(fbo, GL_RED_BITS, minRedBits);
        checkBitCount(fbo, GL_GREEN_BITS, minGreenBits);
        checkBitCount(fbo, GL_BLUE_BITS, minBlueBits);
        checkBitCount(fbo, GL_ALPHA_BITS, minAlphaBits);
        checkBitCount(fbo, GL_DEPTH_BITS, minDepthBits);
        checkBitCount(fbo, GL_STENCIL_BITS, minStencilBits);
    }

    void testTextureFormat(GLenum internalFormat,
                           GLint minRedBits,
                           GLint minGreenBits,
                           GLint minBlueBits,
                           GLint minAlphaBits)
    {
        glGenTextures(1, &mTexture);
        glBindTexture(GL_TEXTURE_2D, mTexture);

        if (getClientMajorVersion() >= 3)
        {
            glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, 1, 1);
        }
        else
        {
            glTexStorage2DEXT(GL_TEXTURE_2D, 1, internalFormat, 1, 1);
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture, 0);

        testBitCounts(mFramebuffer, minRedBits, minGreenBits, minBlueBits, minAlphaBits, 0, 0);
    }

    void testRenderbufferMultisampleFormat(int minESVersion,
                                           GLenum attachmentType,
                                           GLenum internalFormat)
    {
        int clientVersion = getClientMajorVersion();
        if (clientVersion < minESVersion)
        {
            return;
        }

        // Check that multisample is supported with at least two samples (minimum required is 1)
        bool supports2Samples = false;

        if (clientVersion == 2)
        {
            if (IsGLExtensionEnabled("ANGLE_framebuffer_multisample"))
            {
                int maxSamples;
                glGetIntegerv(GL_MAX_SAMPLES_ANGLE, &maxSamples);
                supports2Samples = maxSamples >= 2;
            }
        }
        else
        {
            assert(clientVersion >= 3);
            int maxSamples;
            glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
            supports2Samples = maxSamples >= 2;
        }

        if (!supports2Samples)
        {
            return;
        }

        glGenRenderbuffers(1, &mRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);

        EXPECT_GL_NO_ERROR();
        glRenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, 2, internalFormat, 128, 128);
        EXPECT_GL_NO_ERROR();
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachmentType, GL_RENDERBUFFER, mRenderbuffer);
        EXPECT_GL_NO_ERROR();
    }

    void testZeroHeightRenderbuffer()
    {
        glGenRenderbuffers(1, &mRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  mRenderbuffer);
        EXPECT_GL_NO_ERROR();
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        glGenFramebuffers(1, &mFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    }

    void TearDown() override
    {
        ANGLETest::TearDown();

        if (mTexture != 0)
        {
            glDeleteTextures(1, &mTexture);
            mTexture = 0;
        }

        if (mRenderbuffer != 0)
        {
            glDeleteRenderbuffers(1, &mRenderbuffer);
            mRenderbuffer = 0;
        }

        if (mFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &mFramebuffer);
            mFramebuffer = 0;
        }

        if (mProgram != 0)
        {
            glDeleteProgram(mProgram);
            mProgram = 0;
        }
    }

    GLuint mFramebuffer;
    GLuint mTexture;
    GLuint mRenderbuffer;
    GLuint mProgram;
};

TEST_P(FramebufferFormatsTest, RGBA4)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       !IsGLExtensionEnabled("GL_EXT_texture_storage"));

    testTextureFormat(GL_RGBA4, 4, 4, 4, 4);
}

TEST_P(FramebufferFormatsTest, RGB565)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       !IsGLExtensionEnabled("GL_EXT_texture_storage"));

    testTextureFormat(GL_RGB565, 5, 6, 5, 0);
}

TEST_P(FramebufferFormatsTest, RGB8)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       (!IsGLExtensionEnabled("GL_OES_rgb8_rgba8") ||
                        !IsGLExtensionEnabled("GL_EXT_texture_storage")));

    testTextureFormat(GL_RGB8_OES, 8, 8, 8, 0);
}

TEST_P(FramebufferFormatsTest, BGRA8)
{
    ANGLE_SKIP_TEST_IF(
        !IsGLExtensionEnabled("GL_EXT_texture_format_BGRA8888") ||
        (getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_texture_storage")));

    testTextureFormat(GL_BGRA8_EXT, 8, 8, 8, 8);
}

TEST_P(FramebufferFormatsTest, RGBA8)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       (!IsGLExtensionEnabled("GL_OES_rgb8_rgba8") ||
                        !IsGLExtensionEnabled("GL_EXT_texture_storage")));

    testTextureFormat(GL_RGBA8_OES, 8, 8, 8, 8);
}

TEST_P(FramebufferFormatsTest, RenderbufferMultisample_DEPTH16)
{
    testRenderbufferMultisampleFormat(2, GL_DEPTH_ATTACHMENT, GL_DEPTH_COMPONENT16);
}

TEST_P(FramebufferFormatsTest, RenderbufferMultisample_DEPTH24)
{
    testRenderbufferMultisampleFormat(3, GL_DEPTH_ATTACHMENT, GL_DEPTH_COMPONENT24);
}

TEST_P(FramebufferFormatsTest, RenderbufferMultisample_DEPTH32F)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    testRenderbufferMultisampleFormat(3, GL_DEPTH_ATTACHMENT, GL_DEPTH_COMPONENT32F);
}

TEST_P(FramebufferFormatsTest, RenderbufferMultisample_DEPTH24_STENCIL8)
{
    testRenderbufferMultisampleFormat(3, GL_DEPTH_STENCIL_ATTACHMENT, GL_DEPTH24_STENCIL8);
}

TEST_P(FramebufferFormatsTest, RenderbufferMultisample_DEPTH32F_STENCIL8)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    testRenderbufferMultisampleFormat(3, GL_DEPTH_STENCIL_ATTACHMENT, GL_DEPTH32F_STENCIL8);
}

TEST_P(FramebufferFormatsTest, RenderbufferMultisample_STENCIL_INDEX8)
{
    // TODO(geofflang): Figure out how to support GLSTENCIL_INDEX8 on desktop GL
    ANGLE_SKIP_TEST_IF(IsDesktopOpenGL());

    testRenderbufferMultisampleFormat(2, GL_STENCIL_ATTACHMENT, GL_STENCIL_INDEX8);
}

// Test that binding an incomplete cube map is rejected by ANGLE.
TEST_P(FramebufferFormatsTest, IncompleteCubeMap)
{
    // http://anglebug.com/3145
    ANGLE_SKIP_TEST_IF(IsFuchsia() && IsIntel() && IsVulkan());

    // First make a complete CubeMap.
    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mTexture);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                           mTexture, 0);

    // Verify the framebuffer is complete.
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Make the CubeMap cube-incomplete.
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);

    // Verify the framebuffer is incomplete.
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ASSERT_GL_NO_ERROR();

    // Verify drawing with the incomplete framebuffer produces a GL error
    mProgram = CompileProgram(essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    ASSERT_NE(0u, mProgram);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
}

// Test that a renderbuffer with zero height but nonzero width is handled without crashes/asserts.
TEST_P(FramebufferFormatsTest, ZeroHeightRenderbuffer)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    testZeroHeightRenderbuffer();
}

// Test to cover a bug where the read framebuffer affects the completeness of the draw framebuffer.
TEST_P(FramebufferFormatsTest, ReadDrawCompleteness)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    GLTexture incompleteTexture;
    glBindTexture(GL_TEXTURE_2D, incompleteTexture);

    GLFramebuffer incompleteFBO;
    glBindFramebuffer(GL_FRAMEBUFFER, incompleteFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, incompleteTexture,
                           0);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    GLTexture completeTexture;
    glBindTexture(GL_TEXTURE_2D, completeTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, getWindowWidth(), getWindowHeight());

    GLFramebuffer completeFBO;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, completeFBO);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           completeTexture, 0);

    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_READ_FRAMEBUFFER));
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));

    ASSERT_GL_NO_ERROR();

    // Simple draw program.
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());

    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, completeFBO);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(FramebufferFormatsTest,
                       ES2_VULKAN(),
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES());

class FramebufferTest_ES3 : public ANGLETest
{};

// Covers invalidating an incomplete framebuffer. This should be a no-op, but should not error.
TEST_P(FramebufferTest_ES3, InvalidateIncomplete)
{
    GLFramebuffer framebuffer;
    GLRenderbuffer renderbuffer;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    std::vector<GLenum> attachments;
    attachments.push_back(GL_COLOR_ATTACHMENT0);

    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, attachments.data());
    EXPECT_GL_NO_ERROR();
}

// Test that the framebuffer state tracking robustly handles a depth-only attachment being set
// as a depth-stencil attachment. It is equivalent to detaching the depth-stencil attachment.
TEST_P(FramebufferTest_ES3, DepthOnlyAsDepthStencil)
{
    GLFramebuffer framebuffer;
    GLRenderbuffer renderbuffer;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 4, 4);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuffer);
    EXPECT_GLENUM_NE(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
}

// Test that the framebuffer correctly returns that it is not complete if invalid texture mip levels
// are bound
TEST_P(FramebufferTest_ES3, TextureAttachmentMipLevels)
{
    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    // Create a complete mip chain in mips 1 to 3
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Create another complete mip chain in mips 4 to 5
    glTexImage2D(GL_TEXTURE_2D, 4, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 5, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Create a non-complete mip chain in mip 6
    glTexImage2D(GL_TEXTURE_2D, 6, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Incomplete, mipLevel != baseLevel and texture is not mip complete
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 1);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Complete, mipLevel == baseLevel
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    ExpectFramebufferCompleteOrUnsupported(GL_FRAMEBUFFER);

    // Complete, mipLevel != baseLevel but texture is now mip complete
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 2);
    ExpectFramebufferCompleteOrUnsupported(GL_FRAMEBUFFER);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 3);
    ExpectFramebufferCompleteOrUnsupported(GL_FRAMEBUFFER);

    // Incomplete, attached level below the base level
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 2);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 1);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Incomplete, attached level is beyond effective max level
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 4);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Complete, mipLevel == baseLevel
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 4);
    ExpectFramebufferCompleteOrUnsupported(GL_FRAMEBUFFER);

    // Complete, mipLevel != baseLevel but texture is now mip complete
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 5);
    ExpectFramebufferCompleteOrUnsupported(GL_FRAMEBUFFER);

    // Complete, mipLevel == baseLevel
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 6);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 6);
    ExpectFramebufferCompleteOrUnsupported(GL_FRAMEBUFFER);
}

// Test that passing an attachment COLOR_ATTACHMENTm where m is equal to MAX_COLOR_ATTACHMENTS
// generates an INVALID_OPERATION.
// OpenGL ES Version 3.0.5 (November 3, 2016), 4.4.2.4 Attaching Texture Images to a Framebuffer, p.
// 208
TEST_P(FramebufferTest_ES3, ColorAttachmentIndexOutOfBounds)
{
    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());

    GLint maxColorAttachments = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
    GLenum attachment = static_cast<GLenum>(maxColorAttachments + GL_COLOR_ATTACHMENT0);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture.get());
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 1, 1);
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture.get(), 0);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Check that depth-only attachments report the correct number of samples.
TEST_P(FramebufferTest_ES3, MultisampleDepthOnly)
{
    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_DEPTH_COMPONENT24, 32, 32);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    EXPECT_GL_NO_ERROR();

    GLint samples = 0;
    glGetIntegerv(GL_SAMPLES, &samples);
    EXPECT_GL_NO_ERROR();
    EXPECT_GE(samples, 2);
}

// Check that we only compare width and height of attachments, not depth.
TEST_P(FramebufferTest_ES3, AttachmentWith3DLayers)
{
    GLTexture texA;
    glBindTexture(GL_TEXTURE_2D, texA);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GLTexture texB;
    glBindTexture(GL_TEXTURE_3D, texB);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 4, 4, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texA, 0);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, texB, 0, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    EXPECT_GL_NO_ERROR();
}

// Test that clearing the stencil buffer when the framebuffer only has a color attachment does not
// crash.
TEST_P(FramebufferTest_ES3, ClearNonexistentStencil)
{
    GLRenderbuffer rbo;
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);

    GLint clearValue = 0;
    glClearBufferiv(GL_STENCIL, 0, &clearValue);

    // There's no error specified for clearing nonexistent buffers, it's simply a no-op.
    EXPECT_GL_NO_ERROR();
}

// Test that clearing the depth buffer when the framebuffer only has a color attachment does not
// crash.
TEST_P(FramebufferTest_ES3, ClearNonexistentDepth)
{
    GLRenderbuffer rbo;
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);

    GLfloat clearValue = 0.0f;
    glClearBufferfv(GL_DEPTH, 0, &clearValue);

    // There's no error specified for clearing nonexistent buffers, it's simply a no-op.
    EXPECT_GL_NO_ERROR();
}

// Test that clearing a nonexistent color attachment does not crash.
TEST_P(FramebufferTest_ES3, ClearNonexistentColor)
{
    GLRenderbuffer rbo;
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);

    std::vector<GLfloat> clearValue = {{0.0f, 1.0f, 0.0f, 1.0f}};
    glClearBufferfv(GL_COLOR, 1, clearValue.data());

    // There's no error specified for clearing nonexistent buffers, it's simply a no-op.
    EXPECT_GL_NO_ERROR();
}

// Test that clearing the depth and stencil buffers when the framebuffer only has a color attachment
// does not crash.
TEST_P(FramebufferTest_ES3, ClearNonexistentDepthStencil)
{
    GLRenderbuffer rbo;
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);

    glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.0f, 0);

    // There's no error specified for clearing nonexistent buffers, it's simply a no-op.
    EXPECT_GL_NO_ERROR();
}

// Test that clearing a color attachment that has been deleted doesn't crash.
TEST_P(FramebufferTest_ES3, ClearDeletedAttachment)
{
    // An INVALID_FRAMEBUFFER_OPERATION error was seen in this test on Mac, not sure where it might
    // be originating from. http://anglebug.com/2834
    ANGLE_SKIP_TEST_IF(IsOSX() && IsOpenGL());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // There used to be a bug where some draw buffer state used to remain set even after the
    // attachment was detached via deletion. That's why we create, attach and delete this RBO here.
    GLuint rbo = 0u;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
    glDeleteRenderbuffers(1, &rbo);

    // There needs to be at least one color attachment to prevent early out from the clear calls.
    GLRenderbuffer rbo2;
    glBindRenderbuffer(GL_RENDERBUFFER, rbo2);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, rbo2);

    ASSERT_GL_NO_ERROR();

    // There's no error specified for clearing nonexistent buffers, it's simply a no-op, so we
    // expect no GL errors below.
    std::array<GLfloat, 4> floatClearValue = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearBufferfv(GL_COLOR, 0, floatClearValue.data());
    EXPECT_GL_NO_ERROR();
    std::array<GLuint, 4> uintClearValue = {0u, 0u, 0u, 0u};
    glClearBufferuiv(GL_COLOR, 0, uintClearValue.data());
    EXPECT_GL_NO_ERROR();
    std::array<GLint, 4> intClearValue = {0, 0, 0, 0};
    glClearBufferiv(GL_COLOR, 0, intClearValue.data());
    EXPECT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST(FramebufferTest_ES3, ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES());

class FramebufferTest_ES31 : public ANGLETest
{
  protected:
    void validateSamplePass(GLuint &query, GLuint &passedCount, GLint width, GLint height)
    {
        glUniform2i(0, width - 1, height - 1);
        glBeginQuery(GL_ANY_SAMPLES_PASSED, query);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        glGetQueryObjectuiv(query, GL_QUERY_RESULT, &passedCount);
        EXPECT_GT(static_cast<GLint>(passedCount), 0);

        glUniform2i(0, width - 1, height);
        glBeginQuery(GL_ANY_SAMPLES_PASSED, query);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        glGetQueryObjectuiv(query, GL_QUERY_RESULT, &passedCount);
        EXPECT_EQ(static_cast<GLint>(passedCount), 0);

        glUniform2i(0, width, height - 1);
        glBeginQuery(GL_ANY_SAMPLES_PASSED, query);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        glGetQueryObjectuiv(query, GL_QUERY_RESULT, &passedCount);
        EXPECT_EQ(static_cast<GLint>(passedCount), 0);
    }
};

// Test that without attachment, if either the value of FRAMEBUFFER_DEFAULT_WIDTH or
// FRAMEBUFFER_DEFAULT_HEIGHT parameters is zero, the framebuffer is incomplete.
TEST_P(FramebufferTest_ES31, IncompleteMissingAttachmentDefaultParam)
{
    GLFramebuffer mFramebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer.get());

    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, 1);
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, 1);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, 0);
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, 0);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, 1);
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, 0);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, 0);
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, 1);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ASSERT_GL_NO_ERROR();
}

// Test that the sample count of a mix of texture and renderbuffer should be same.
TEST_P(FramebufferTest_ES31, IncompleteMultisampleSampleCountMix)
{
    GLFramebuffer mFramebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer.get());

    GLTexture mTexture;
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mTexture.get());
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, 1, 1, true);

    GLRenderbuffer mRenderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer.get());
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_RGBA8, 1, 1);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                           mTexture.get(), 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER,
                              mRenderbuffer.get());
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ASSERT_GL_NO_ERROR();
}

// Test that the sample count of texture attachments should be same.
TEST_P(FramebufferTest_ES31, IncompleteMultisampleSampleCountTex)
{
    GLFramebuffer mFramebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer.get());

    GLTexture mTextures[2];
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mTextures[0].get());
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, 1, 1, true);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mTextures[1].get());
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 2, GL_RGBA8, 1, 1, true);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                           mTextures[0].get(), 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D_MULTISAMPLE,
                           mTextures[1].get(), 0);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ASSERT_GL_NO_ERROR();
}

// Test that if the attached images are a mix of renderbuffers and textures, the value of
// TEXTURE_FIXED_SAMPLE_LOCATIONS must be TRUE for all attached textures.
TEST_P(FramebufferTest_ES31, IncompleteMultisampleFixedSampleLocationsMix)
{
    GLFramebuffer mFramebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer.get());

    GLTexture mTexture;
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mTexture.get());
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, 1, 1, false);

    GLRenderbuffer mRenderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer.get());
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 1, GL_RGBA8, 1, 1);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                           mTexture.get(), 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER,
                              mRenderbuffer.get());
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ASSERT_GL_NO_ERROR();
}

// Test that the value of TEXTURE_FIXED_SAMPLE_LOCATIONS is the same for all attached textures.
TEST_P(FramebufferTest_ES31, IncompleteMultisampleFixedSampleLocationsTex)
{
    GLFramebuffer mFramebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer.get());

    GLTexture mTextures[2];
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mTextures[0].get());
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, 1, 1, false);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                           mTextures[0].get(), 0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mTextures[1].get());
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGB8, 1, 1, true);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D_MULTISAMPLE,
                           mTextures[1].get(), 0);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ASSERT_GL_NO_ERROR();
}

// If there are no attachments, rendering will be limited to a rectangle having a lower left of
// (0, 0) and an upper right of(width, height), where width and height are the framebuffer
// object's default width and height.
TEST_P(FramebufferTest_ES31, RenderingLimitToDefaultFBOSizeWithNoAttachments)
{
    // anglebug.com/2253
    ANGLE_SKIP_TEST_IF(IsLinux() && IsAMD() && IsDesktopOpenGL());

    constexpr char kVS1[] = R"(#version 310 es
in layout(location = 0) highp vec2 a_position;
void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
})";

    constexpr char kFS1[] = R"(#version 310 es
uniform layout(location = 0) highp ivec2 u_expectedSize;
out layout(location = 5) mediump vec4 f_color;
void main()
{
    if (ivec2(gl_FragCoord.xy) != u_expectedSize) discard;
    f_color = vec4(1.0, 0.5, 0.25, 1.0);
})";

    constexpr char kVS2[] = R"(#version 310 es
in layout(location = 0) highp vec2 a_position;
void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
})";

    constexpr char kFS2[] = R"(#version 310 es
uniform layout(location = 0) highp ivec2 u_expectedSize;
out layout(location = 2) mediump vec4 f_color;
void main()
{
    if (ivec2(gl_FragCoord.xy) != u_expectedSize) discard;
    f_color = vec4(1.0, 0.5, 0.25, 1.0);
})";

    GLuint program1 = CompileProgram(kVS1, kFS1);
    ASSERT_NE(program1, 0u);

    GLuint program2 = CompileProgram(kVS2, kFS2);
    ASSERT_NE(program2, 0u);

    glUseProgram(program1);

    GLFramebuffer mFramebuffer;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFramebuffer);
    GLuint defaultWidth  = 1;
    GLuint defaultHeight = 1;

    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, defaultWidth);
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, defaultHeight);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    const float data[] = {
        1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f,
    };

    GLuint vertexArray  = 0;
    GLuint vertexBuffer = 0;
    GLuint query        = 0;
    GLuint passedCount  = 0;

    glGenQueries(1, &query);
    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);

    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, false, 0, 0);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    validateSamplePass(query, passedCount, defaultWidth, defaultHeight);

    glUseProgram(program2);
    validateSamplePass(query, passedCount, defaultWidth, defaultHeight);

    glUseProgram(program1);
    // If fbo has attachments, the rendering size should be the same as its attachment.
    GLTexture mTexture;
    GLuint width  = 2;
    GLuint height = 2;
    glBindTexture(GL_TEXTURE_2D, mTexture.get());
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

    const GLenum bufs[] = {GL_NONE, GL_NONE, GL_NONE, GL_NONE, GL_NONE, GL_COLOR_ATTACHMENT5};

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, mTexture.get(),
                           0);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    glDrawBuffers(6, bufs);

    validateSamplePass(query, passedCount, width, height);

    // If fbo's attachment has been removed, the rendering size should be the same as framebuffer
    // default size.
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, 0, 0, 0);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    validateSamplePass(query, passedCount, defaultWidth, defaultHeight);

    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &vertexBuffer);
    glDeleteVertexArrays(1, &vertexArray);

    ASSERT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST(FramebufferTest_ES31, ES31_D3D11(), ES31_OPENGL(), ES31_OPENGLES());

class AddDummyTextureNoRenderTargetTest : public ANGLETest
{
  public:
    AddDummyTextureNoRenderTargetTest()
    {
        setWindowWidth(512);
        setWindowHeight(512);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void overrideWorkaroundsD3D(WorkaroundsD3D *workarounds) override
    {
        workarounds->addDummyTextureNoRenderTarget = true;
    }
};

// Test to verify workaround succeeds when no program outputs exist http://anglebug.com/2283
TEST_P(AddDummyTextureNoRenderTargetTest, NoProgramOutputWorkaround)
{
    constexpr char kVS[] = "void main() {}";
    constexpr char kFS[] = "void main() {}";

    ANGLE_GL_PROGRAM(drawProgram, kVS, kFS);

    glUseProgram(drawProgram);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    ASSERT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST(AddDummyTextureNoRenderTargetTest, ES2_D3D11());
