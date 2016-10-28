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

}  // anonymous namespace

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
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB8_ETC2, 16, 16, 0, 64, nullptr);
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

    // Draw once to set the RenderTarget in D3D11
    GLColor red(255, 0, 0, 255);
    setUniformColor(red);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, red);

    // This will trigger the texture to be re-created on FL9_3.
    glGenerateMipmap(GL_TEXTURE_2D);

    // Now ensure we don't have a stale render target.
    GLColor blue(0, 0, 255, 255);
    setUniformColor(blue);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, blue);

    EXPECT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST(StateChangeTest, ES2_D3D9(), ES2_D3D11(), ES2_OPENGL());
ANGLE_INSTANTIATE_TEST(StateChangeRenderTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES2_OPENGL(),
                       ES2_D3D11_FL9_3());
ANGLE_INSTANTIATE_TEST(StateChangeTestES3, ES3_D3D11(), ES3_OPENGL());
