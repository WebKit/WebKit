//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// StateChangeTest:
//   Specifically designed for an ANGLE implementation of GL, these tests validate that
//   ANGLE's dirty bits systems don't get confused by certain sequences of state changes.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class StateChangeTest : public ANGLETest
{
  protected:
    StateChangeTest()
    {
        setWindowWidth(64);
        setWindowHeight(64);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);

        // Enable the no error extension to avoid syncing the FBO state on validation.
        setNoErrorEnabled(true);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        glGenFramebuffers(1, &mFramebuffer);
        glGenTextures(2, mTextures.data());
        glGenRenderbuffers(1, &mRenderbuffer);

        ASSERT_GL_NO_ERROR();
    }

    void TearDown() override
    {
        if (mFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &mFramebuffer);
            mFramebuffer = 0;
        }

        if (!mTextures.empty())
        {
            glDeleteTextures(static_cast<GLsizei>(mTextures.size()), mTextures.data());
            mTextures.clear();
        }

        glDeleteRenderbuffers(1, &mRenderbuffer);

        ANGLETest::TearDown();
    }

    GLuint mFramebuffer           = 0;
    GLuint mRenderbuffer          = 0;
    std::vector<GLuint> mTextures = {0, 0};
};

class StateChangeTestES3 : public StateChangeTest
{
  protected:
    StateChangeTestES3() {}
};

// Ensure that CopyTexImage2D syncs framebuffer changes.
TEST_P(StateChangeTest, CopyTexImage2DSync)
{
    if (IsAMD() && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
    {
        // TODO(geofflang): Fix on Linux AMD drivers (http://anglebug.com/1291)
        std::cout << "Test disabled on AMD OpenGL." << std::endl;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

    // Init first texture to red
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    // Init second texture to green
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1], 0);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 0, 255, 0, 255);

    // Copy in the red texture to the green one.
    // CopyTexImage should sync the framebuffer attachment change.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 16, 16, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1], 0);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    ASSERT_GL_NO_ERROR();
}

// Ensure that CopyTexSubImage2D syncs framebuffer changes.
TEST_P(StateChangeTest, CopyTexSubImage2DSync)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

    // Init first texture to red
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    // Init second texture to green
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1], 0);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 0, 255, 0, 255);

    // Copy in the red texture to the green one.
    // CopyTexImage should sync the framebuffer attachment change.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 16, 16);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1], 0);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    ASSERT_GL_NO_ERROR();
}

// Test that Framebuffer completeness caching works when color attachments change.
TEST_P(StateChangeTest, FramebufferIncompleteColorAttachment)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Change the texture at color attachment 0 to be non-color-renderable.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 16, 16, 0, GL_ALPHA, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ASSERT_GL_NO_ERROR();
}

// Test that caching works when color attachments change with TexStorage.
TEST_P(StateChangeTest, FramebufferIncompleteWithTexStorage)
{
    if (!extensionEnabled("GL_EXT_texture_storage"))
    {
        std::cout << "Test skipped because TexStorage2DEXT not available." << std::endl;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Change the texture at color attachment 0 to be non-color-renderable.
    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_ALPHA8_EXT, 16, 16);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ASSERT_GL_NO_ERROR();
}

// Test that caching works when color attachments change with CompressedTexImage2D.
TEST_P(StateChangeTestES3, FramebufferIncompleteWithCompressedTex)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Change the texture at color attachment 0 to be non-color-renderable.
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB8_ETC2, 16, 16, 0, 128, nullptr);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ASSERT_GL_NO_ERROR();
}

// Test that caching works when color attachments are deleted.
TEST_P(StateChangeTestES3, FramebufferIncompleteWhenAttachmentDeleted)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Delete the texture at color attachment 0.
    glDeleteTextures(1, &mTextures[0]);
    mTextures[0] = 0;
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ASSERT_GL_NO_ERROR();
}

// Test that Framebuffer completeness caching works when depth attachments change.
TEST_P(StateChangeTest, FramebufferIncompleteDepthAttachment)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 16, 16);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mRenderbuffer);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Change the texture at color attachment 0 to be non-depth-renderable.
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ASSERT_GL_NO_ERROR();
}

// Test that Framebuffer completeness caching works when stencil attachments change.
TEST_P(StateChangeTest, FramebufferIncompleteStencilAttachment)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, 16, 16);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              mRenderbuffer);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Change the texture at the stencil attachment to be non-stencil-renderable.
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ASSERT_GL_NO_ERROR();
}

// Test that Framebuffer completeness caching works when depth-stencil attachments change.
TEST_P(StateChangeTest, FramebufferIncompleteDepthStencilAttachment)
{
    if (getClientMajorVersion() < 3 && !extensionEnabled("GL_OES_packed_depth_stencil"))
    {
        std::cout << "Test skipped because packed depth+stencil not availble." << std::endl;
        return;
    }

    if (IsWindows() && IsIntel() && IsOpenGL())
    {
        // TODO(jmadill): Investigate the failure (https://anglebug.com/1388)
        std::cout << "Test disabled on Windows Intel OpenGL." << std::endl;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 16, 16);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              mRenderbuffer);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Change the texture the depth-stencil attachment to be non-depth-stencil-renderable.
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ASSERT_GL_NO_ERROR();
}

// Ensure that CopyTexSubImage3D syncs framebuffer changes.
TEST_P(StateChangeTestES3, CopyTexSubImage3DSync)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

    // Init first texture to red
    glBindTexture(GL_TEXTURE_3D, mTextures[0]);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 16, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTextures[0], 0, 0);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    // Init second texture to green
    glBindTexture(GL_TEXTURE_3D, mTextures[1]);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 16, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTextures[1], 0, 0);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 0, 255, 0, 255);

    // Copy in the red texture to the green one.
    // CopyTexImage should sync the framebuffer attachment change.
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTextures[0], 0, 0);
    glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 16, 16);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTextures[1], 0, 0);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    ASSERT_GL_NO_ERROR();
}

// Ensure that BlitFramebuffer syncs framebuffer changes.
TEST_P(StateChangeTestES3, BlitFramebufferSync)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

    // Init first texture to red
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    // Init second texture to green
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1], 0);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 0, 255, 0, 255);

    // Change to the red textures and blit.
    // BlitFramebuffer should sync the framebuffer attachment change.
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0],
                           0);
    glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    ASSERT_GL_NO_ERROR();
}

// Ensure that ReadBuffer and DrawBuffers sync framebuffer changes.
TEST_P(StateChangeTestES3, ReadBufferAndDrawBuffersSync)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

    // Initialize two FBO attachments
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mTextures[1], 0);

    // Clear first attachment to red
    GLenum bufs1[] = {GL_COLOR_ATTACHMENT0, GL_NONE};
    glDrawBuffers(2, bufs1);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Clear second texture to green
    GLenum bufs2[] = {GL_NONE, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, bufs2);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Verify first attachment is red and second is green
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    EXPECT_PIXEL_EQ(0, 0, 0, 255, 0, 255);

    glReadBuffer(GL_COLOR_ATTACHMENT0);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    ASSERT_GL_NO_ERROR();
}

// Tests calling invalidate on incomplete framebuffers after switching attachments.
// Adapted partially from WebGL 2 test "renderbuffers/invalidate-framebuffer"
TEST_P(StateChangeTestES3, IncompleteRenderbufferAttachmentInvalidateSync)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);
    GLint samples = 0;
    glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_SAMPLES, 1, &samples);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mRenderbuffer);
    ASSERT_GL_NO_ERROR();

    // invalidate the framebuffer when the attachment is incomplete: no storage allocated to the
    // attached renderbuffer
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));
    GLenum attachments1[] = {GL_COLOR_ATTACHMENT0};
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, attachments1);
    ASSERT_GL_NO_ERROR();

    glRenderbufferStorageMultisample(GL_RENDERBUFFER, static_cast<GLsizei>(samples), GL_RGBA8,
                                     getWindowWidth(), getWindowHeight());
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    GLRenderbuffer renderbuf;

    glBindRenderbuffer(GL_RENDERBUFFER, renderbuf.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuf.get());
    ASSERT_GL_NO_ERROR();

    // invalidate the framebuffer when the attachment is incomplete: no storage allocated to the
    // attached renderbuffer
    // Note: the bug will only repro *without* a call to checkStatus before the invalidate.
    GLenum attachments2[] = {GL_DEPTH_ATTACHMENT};
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, attachments2);

    glRenderbufferStorageMultisample(GL_RENDERBUFFER, static_cast<GLsizei>(samples),
                                     GL_DEPTH_COMPONENT16, getWindowWidth(), getWindowHeight());
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    glClear(GL_DEPTH_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();
}

class StateChangeRenderTest : public StateChangeTest
{
  protected:
    StateChangeRenderTest() : mProgram(0), mRenderbuffer(0) {}

    void SetUp() override
    {
        StateChangeTest::SetUp();

        const std::string vertexShaderSource =
            "attribute vec2 position;\n"
            "void main() {\n"
            "    gl_Position = vec4(position, 0, 1);\n"
            "}";
        const std::string fragmentShaderSource =
            "uniform highp vec4 uniformColor;\n"
            "void main() {\n"
            "    gl_FragColor = uniformColor;\n"
            "}";

        mProgram = CompileProgram(vertexShaderSource, fragmentShaderSource);
        ASSERT_NE(0u, mProgram);

        glGenRenderbuffers(1, &mRenderbuffer);
    }

    void TearDown() override
    {
        glDeleteProgram(mProgram);
        glDeleteRenderbuffers(1, &mRenderbuffer);

        StateChangeTest::TearDown();
    }

    void setUniformColor(const GLColor &color)
    {
        glUseProgram(mProgram);
        const Vector4 &normalizedColor = color.toNormalizedVector();
        GLint uniformLocation = glGetUniformLocation(mProgram, "uniformColor");
        ASSERT_NE(-1, uniformLocation);
        glUniform4fv(uniformLocation, 1, normalizedColor.data());
    }

    GLuint mProgram;
    GLuint mRenderbuffer;
};

// Test that re-creating a currently attached texture works as expected.
TEST_P(StateChangeRenderTest, RecreateTexture)
{
    if (IsIntel() && IsLinux())
    {
        // TODO(cwallez): Fix on Linux Intel drivers (http://anglebug.com/1346)
        std::cout << "Test disabled on Linux Intel OpenGL." << std::endl;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);

    // Explictly check FBO status sync in some versions of ANGLE no_error skips FBO checks.
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Draw with red to the FBO.
    GLColor red(255, 0, 0, 255);
    setUniformColor(red);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, red);

    // Recreate the texture with green.
    GLColor green(0, 255, 0, 255);
    std::vector<GLColor> greenPixels(32 * 32, green);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 greenPixels.data());
    EXPECT_PIXEL_COLOR_EQ(0, 0, green);

    // Explictly check FBO status sync in some versions of ANGLE no_error skips FBO checks.
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Verify drawing blue gives blue. This covers the FBO sync with D3D dirty bits.
    GLColor blue(0, 0, 255, 255);
    setUniformColor(blue);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, blue);

    EXPECT_GL_NO_ERROR();
}

// Test that re-creating a currently attached renderbuffer works as expected.
TEST_P(StateChangeRenderTest, RecreateRenderbuffer)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

    glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mRenderbuffer);

    // Explictly check FBO status sync in some versions of ANGLE no_error skips FBO checks.
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Draw with red to the FBO.
    GLColor red(255, 0, 0, 255);
    setUniformColor(red);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, red);

    // Recreate the renderbuffer and clear to green.
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 32, 32);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    GLColor green(0, 255, 0, 255);
    EXPECT_PIXEL_COLOR_EQ(0, 0, green);

    // Explictly check FBO status sync in some versions of ANGLE no_error skips FBO checks.
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Verify drawing blue gives blue. This covers the FBO sync with D3D dirty bits.
    GLColor blue(0, 0, 255, 255);
    setUniformColor(blue);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, blue);

    EXPECT_GL_NO_ERROR();
}

// Test that recreating a texture with GenerateMipmaps signals the FBO is dirty.
TEST_P(StateChangeRenderTest, GenerateMipmap)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);

    // Explictly check FBO status sync in some versions of ANGLE no_error skips FBO checks.
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Draw once to set the RenderTarget in D3D11
    GLColor red(255, 0, 0, 255);
    setUniformColor(red);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, red);

    // This will trigger the texture to be re-created on FL9_3.
    glGenerateMipmap(GL_TEXTURE_2D);

    // Explictly check FBO status sync in some versions of ANGLE no_error skips FBO checks.
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Now ensure we don't have a stale render target.
    GLColor blue(0, 0, 255, 255);
    setUniformColor(blue);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, blue);

    EXPECT_GL_NO_ERROR();
}

// Tests that D3D11 dirty bit updates don't forget about BufferSubData attrib updates.
TEST_P(StateChangeTest, VertexBufferUpdatedAfterDraw)
{
    const std::string vs =
        "attribute vec2 position;\n"
        "attribute vec4 color;\n"
        "varying vec4 outcolor;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 0, 1);\n"
        "    outcolor = color;\n"
        "}";
    const std::string fs =
        "varying mediump vec4 outcolor;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = outcolor;\n"
        "}";

    ANGLE_GL_PROGRAM(program, vs, fs);
    glUseProgram(program);

    GLint colorLoc = glGetAttribLocation(program, "color");
    ASSERT_NE(-1, colorLoc);
    GLint positionLoc = glGetAttribLocation(program, "position");
    ASSERT_NE(-1, positionLoc);

    setupQuadVertexBuffer(0.5f, 1.0f);
    glEnableVertexAttribArray(positionLoc);
    glVertexAttribPointer(positionLoc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    GLBuffer colorBuf;
    glBindBuffer(GL_ARRAY_BUFFER, colorBuf);
    glVertexAttribPointer(colorLoc, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, nullptr);
    glEnableVertexAttribArray(colorLoc);

    // Fill with green.
    std::vector<GLColor> colorData(6, GLColor::green);
    glBufferData(GL_ARRAY_BUFFER, colorData.size() * sizeof(GLColor), colorData.data(),
                 GL_STATIC_DRAW);

    // Draw, expect green.
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    ASSERT_GL_NO_ERROR();

    // Update buffer with red.
    std::fill(colorData.begin(), colorData.end(), GLColor::red);
    glBufferSubData(GL_ARRAY_BUFFER, 0, colorData.size() * sizeof(GLColor), colorData.data());

    // Draw, expect red.
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    ASSERT_GL_NO_ERROR();
}

// Test that switching VAOs keeps the disabled "current value" attributes up-to-date.
TEST_P(StateChangeTestES3, VertexArrayObjectAndDisabledAttributes)
{
    const std::string singleVertexShader =
        "attribute vec4 position; void main() { gl_Position = position; }";
    const std::string singleFragmentShader = "void main() { gl_FragColor = vec4(1, 0, 0, 1); }";
    ANGLE_GL_PROGRAM(singleProgram, singleVertexShader, singleFragmentShader);

    const std::string dualVertexShader =
        "#version 300 es\n"
        "in vec4 position;\n"
        "in vec4 color;\n"
        "out vec4 varyColor;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = position;\n"
        "    varyColor = color;\n"
        "}";
    const std::string dualFragmentShader =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in vec4 varyColor;\n"
        "out vec4 colorOut;\n"
        "void main()\n"
        "{\n"
        "    colorOut = varyColor;\n"
        "}";
    ANGLE_GL_PROGRAM(dualProgram, dualVertexShader, dualFragmentShader);
    GLint positionLocation = glGetAttribLocation(dualProgram, "position");
    ASSERT_NE(-1, positionLocation);
    GLint colorLocation = glGetAttribLocation(dualProgram, "color");
    ASSERT_NE(-1, colorLocation);

    GLint singlePositionLocation = glGetAttribLocation(singleProgram, "position");
    ASSERT_NE(-1, singlePositionLocation);

    glUseProgram(singleProgram);

    // Initialize position vertex buffer.
    const auto &quadVertices = GetQuadVertices();

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vector3) * 6, quadVertices.data(), GL_STATIC_DRAW);

    // Initialize a VAO. Draw with single program.
    GLVertexArray vertexArray;
    glBindVertexArray(vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glVertexAttribPointer(singlePositionLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(singlePositionLocation);

    // Should draw red.
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Draw with a green buffer attribute, without the VAO.
    glBindVertexArray(0);
    glUseProgram(dualProgram);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(positionLocation);

    std::vector<GLColor> greenColors(6, GLColor::green);
    GLBuffer greenBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, greenBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLColor) * 6, greenColors.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(colorLocation, 4, GL_UNSIGNED_BYTE, GL_FALSE, 4, nullptr);
    glEnableVertexAttribArray(colorLocation);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Re-bind VAO and try to draw with different program, without changing state.
    // Should draw black since current value is not initialized.
    glBindVertexArray(vertexArray);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
}

// Simple state change tests, primarily focused on basic object lifetime and dependency management
// with back-ends that don't support that automatically (i.e. Vulkan).
class SimpleStateChangeTest : public ANGLETest
{
  protected:
    SimpleStateChangeTest()
    {
        setWindowWidth(64);
        setWindowHeight(64);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void simpleDrawWithBuffer(GLBuffer *buffer);
    void simpleDrawWithColor(const GLColor &color);
};

constexpr char kSimpleVertexShader[] = R"(attribute vec2 position;
attribute vec4 color;
varying vec4 vColor;
void main()
{
    gl_Position = vec4(position, 0, 1);
    vColor = color;
}
)";

constexpr char kSimpleFragmentShader[] = R"(precision mediump float;
varying vec4 vColor;
void main()
{
    gl_FragColor = vColor;
}
)";

void SimpleStateChangeTest::simpleDrawWithBuffer(GLBuffer *buffer)
{
    ANGLE_GL_PROGRAM(program, kSimpleVertexShader, kSimpleFragmentShader);
    glUseProgram(program);

    GLint colorLoc = glGetAttribLocation(program, "color");
    ASSERT_NE(-1, colorLoc);

    glBindBuffer(GL_ARRAY_BUFFER, *buffer);
    glVertexAttribPointer(colorLoc, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, nullptr);
    glEnableVertexAttribArray(colorLoc);

    drawQuad(program, "position", 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();
}

void SimpleStateChangeTest::simpleDrawWithColor(const GLColor &color)
{
    std::vector<GLColor> colors(6, color);
    GLBuffer colorBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(GLColor), colors.data(), GL_STATIC_DRAW);
    simpleDrawWithBuffer(&colorBuffer);
}

// Handles deleting a Buffer when it's being used.
TEST_P(SimpleStateChangeTest, DeleteBufferInUse)
{
    std::vector<GLColor> colorData(6, GLColor::red);

    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLColor) * colorData.size(), colorData.data(),
                 GL_STATIC_DRAW);

    simpleDrawWithBuffer(&buffer);

    buffer.reset();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// Tests that resizing a Buffer during a draw works as expected.
TEST_P(SimpleStateChangeTest, RedefineBufferInUse)
{
    std::vector<GLColor> redColorData(6, GLColor::red);

    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLColor) * redColorData.size(), redColorData.data(),
                 GL_STATIC_DRAW);

    // Trigger a pull from the buffer.
    simpleDrawWithBuffer(&buffer);

    // Redefine the buffer that's in-flight.
    std::vector<GLColor> greenColorData(1024, GLColor::green);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLColor) * greenColorData.size(), greenColorData.data(),
                 GL_STATIC_DRAW);

    // Trigger the flush and verify the first draw worked.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Draw again and verify the new data is correct.
    simpleDrawWithBuffer(&buffer);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests updating a buffer's contents while in use, without redefining it.
TEST_P(SimpleStateChangeTest, UpdateBufferInUse)
{
    std::vector<GLColor> redColorData(6, GLColor::red);

    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLColor) * redColorData.size(), redColorData.data(),
                 GL_STATIC_DRAW);

    // Trigger a pull from the buffer.
    simpleDrawWithBuffer(&buffer);

    // Update the buffer that's in-flight.
    std::vector<GLColor> greenColorData(6, GLColor::green);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLColor) * greenColorData.size(),
                    greenColorData.data());

    // Trigger the flush and verify the first draw worked.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Draw again and verify the new data is correct.
    simpleDrawWithBuffer(&buffer);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests that deleting an in-flight Texture does not immediately delete the resource.
TEST_P(SimpleStateChangeTest, DeleteTextureInUse)
{
    std::array<GLColor, 4> colors = {
        {GLColor::red, GLColor::green, GLColor::blue, GLColor::yellow}};

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    draw2DTexturedQuad(0.5f, 1.0f, true);
    tex.reset();
    EXPECT_GL_NO_ERROR();

    int w = getWindowWidth() - 2;
    int h = getWindowHeight() - 2;

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(w, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, h, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(w, h, GLColor::yellow);
}

// Tests that redefining an in-flight Texture does not affect the in-flight resource.
TEST_P(SimpleStateChangeTest, RedefineTextureInUse)
{
    std::array<GLColor, 4> colors = {
        {GLColor::red, GLColor::green, GLColor::blue, GLColor::yellow}};

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Draw with the first texture.
    draw2DTexturedQuad(0.5f, 1.0f, true);

    // Redefine the in-flight texture.
    constexpr int kBigSize = 32;
    std::vector<GLColor> bigColors;
    for (int y = 0; y < kBigSize; ++y)
    {
        for (int x = 0; x < kBigSize; ++x)
        {
            bool xComp = x < kBigSize / 2;
            bool yComp = y < kBigSize / 2;
            if (yComp)
            {
                bigColors.push_back(xComp ? GLColor::cyan : GLColor::magenta);
            }
            else
            {
                bigColors.push_back(xComp ? GLColor::yellow : GLColor::white);
            }
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, bigColors.data());
    EXPECT_GL_NO_ERROR();

    // Verify the first draw had the correct data via ReadPixels.
    int w = getWindowWidth() - 2;
    int h = getWindowHeight() - 2;

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(w, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, h, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(w, h, GLColor::yellow);

    // Draw and verify with the redefined data.
    draw2DTexturedQuad(0.5f, 1.0f, true);
    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::cyan);
    EXPECT_PIXEL_COLOR_EQ(w, 0, GLColor::magenta);
    EXPECT_PIXEL_COLOR_EQ(0, h, GLColor::yellow);
    EXPECT_PIXEL_COLOR_EQ(w, h, GLColor::white);
}

const char kSolidColorVertexShader[] = R"(attribute vec2 position;
void main()
{
    gl_Position = vec4(position, 0, 1);
})";

const char kSolidColorFragmentShader[] = R"(void main()
{
    gl_FragColor = vec4(1, 0, 0, 1);
})";

// Tests deleting a Framebuffer that is in use.
TEST_P(SimpleStateChangeTest, DeleteFramebufferInUse)
{
    constexpr int kSize = 16;

    // Create a simple framebuffer.
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glViewport(0, 0, kSize, kSize);

    // Draw a solid red color to the framebuffer.
    ANGLE_GL_PROGRAM(program, kSolidColorVertexShader, kSolidColorFragmentShader);
    drawQuad(program, "position", 0.5f, 1.0f, true);

    // Delete the framebuffer while the call is in flight.
    framebuffer.reset();

    // Make a new framebuffer so we can read back the texture.
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Flush via ReadPixels and check red was drawn.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    ASSERT_GL_NO_ERROR();
}

// Tests deleting a Framebuffer that is in use.
TEST_P(SimpleStateChangeTest, RedefineFramebufferInUse)
{
    constexpr int kSize = 16;

    // Create a simple framebuffer.
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glViewport(0, 0, kSize, kSize);

    // Draw red to the framebuffer.
    simpleDrawWithColor(GLColor::red);

    // Change the framebuffer while the call is in flight to a new texture.
    GLTexture otherTexture;
    glBindTexture(GL_TEXTURE_2D, otherTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, otherTexture, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Draw green to the framebuffer. Verify the color.
    simpleDrawWithColor(GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Make a new framebuffer so we can read back the first texture and verify red.
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    ASSERT_GL_NO_ERROR();
}

}  // anonymous namespace

ANGLE_INSTANTIATE_TEST(StateChangeTest, ES2_D3D9(), ES2_D3D11(), ES2_OPENGL());
ANGLE_INSTANTIATE_TEST(StateChangeRenderTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES2_OPENGL(),
                       ES2_D3D11_FL9_3());
ANGLE_INSTANTIATE_TEST(StateChangeTestES3, ES3_D3D11(), ES3_OPENGL());

ANGLE_INSTANTIATE_TEST(SimpleStateChangeTest, ES2_VULKAN(), ES2_OPENGL());
