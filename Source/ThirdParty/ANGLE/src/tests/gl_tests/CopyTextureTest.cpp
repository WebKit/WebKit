//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// CopyTextureTest.cpp: Tests of the GL_CHROMIUM_copy_texture extension

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"

namespace angle
{

class CopyTextureTest : public ANGLETest
{
  protected:
    CopyTextureTest()
    {
        setWindowWidth(256);
        setWindowHeight(256);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        glGenTextures(2, mTextures);
        glBindTexture(GL_TEXTURE_2D, mTextures[1]);

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glGenFramebuffers(1, &mFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1],
                               0);

        if (extensionEnabled("GL_CHROMIUM_copy_texture"))
        {
            glCopyTextureCHROMIUM = reinterpret_cast<PFNGLCOPYTEXTURECHROMIUMPROC>(
                eglGetProcAddress("glCopyTextureCHROMIUM"));
            glCopySubTextureCHROMIUM = reinterpret_cast<PFNGLCOPYSUBTEXTURECHROMIUMPROC>(
                eglGetProcAddress("glCopySubTextureCHROMIUM"));
        }
    }

    void TearDown() override
    {
        glDeleteTextures(2, mTextures);
        glDeleteFramebuffers(1, &mFramebuffer);

        ANGLETest::TearDown();
    }

    bool checkExtensions() const
    {
        if (!extensionEnabled("GL_CHROMIUM_copy_texture"))
        {
            std::cout << "Test skipped because GL_CHROMIUM_copy_texture is not available."
                      << std::endl;
            return false;
        }

        EXPECT_NE(nullptr, glCopyTextureCHROMIUM);
        EXPECT_NE(nullptr, glCopySubTextureCHROMIUM);
        return true;
    }

    GLuint mTextures[2] = {
        0, 0,
    };
    GLuint mFramebuffer = 0;

    PFNGLCOPYTEXTURECHROMIUMPROC glCopyTextureCHROMIUM       = nullptr;
    PFNGLCOPYSUBTEXTURECHROMIUMPROC glCopySubTextureCHROMIUM = nullptr;
};

class CopyTextureTestDest : public CopyTextureTest
{
};

class CopyTextureTestES3 : public CopyTextureTest
{
};

// Test to ensure that the basic functionality of the extension works.
TEST_P(CopyTextureTest, BasicCopyTexture)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor pixels = GLColor::red;

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixels);

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, pixels);
}

// Test to ensure that the basic functionality of the extension works.
TEST_P(CopyTextureTest, BasicCopySubTexture)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor pixels = GLColor::red;

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixels);

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glCopySubTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, 0, 0, 0, 0, 1, 1,
                             false, false, false);

    EXPECT_GL_NO_ERROR();

    // Check that FB is complete.
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    EXPECT_PIXEL_COLOR_EQ(0, 0, pixels);

    EXPECT_GL_NO_ERROR();
}

// Test that CopyTexture cannot redefine an immutable texture and CopySubTexture can copy data to
// immutable textures
TEST_P(CopyTextureTest, ImmutableTexture)
{
    if (!checkExtensions())
    {
        return;
    }

    if (getClientMajorVersion() < 3 &&
        (!extensionEnabled("GL_EXT_texture_storage") || !extensionEnabled("GL_OES_rgb8_rgba8")))
    {
        std::cout
            << "Test skipped due to missing ES3 or GL_EXT_texture_storage or GL_OES_rgb8_rgba8"
            << std::endl;
        return;
    }

    GLColor pixels = GLColor::red;

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 1, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixels);

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 1, 1);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1], 0);
    EXPECT_GL_NO_ERROR();

    // Should generate an error when the texture is redefined
    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Should succeed when using CopySubTexture
    glCopySubTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, 0, 0, 0, 0, 1, 1,
                             false, false, false);
    EXPECT_GL_NO_ERROR();

    // Check that FB is complete.
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    EXPECT_PIXEL_COLOR_EQ(0, 0, pixels);

    EXPECT_GL_NO_ERROR();
}

// Test validation of internal formats in CopyTexture and CopySubTexture
TEST_P(CopyTextureTest, InternalFormat)
{
    if (!checkExtensions())
    {
        return;
    }

    std::vector<GLint> sourceFormats;
    sourceFormats.push_back(GL_ALPHA);
    sourceFormats.push_back(GL_RGB);
    sourceFormats.push_back(GL_RGBA);
    sourceFormats.push_back(GL_LUMINANCE);
    sourceFormats.push_back(GL_LUMINANCE_ALPHA);

    std::vector<GLint> destFormats;
    destFormats.push_back(GL_RGB);
    destFormats.push_back(GL_RGBA);

    if (extensionEnabled("GL_EXT_texture_format_BGRA8888"))
    {
        sourceFormats.push_back(GL_BGRA_EXT);
        destFormats.push_back(GL_BGRA_EXT);
    }

    // Test with glCopyTexture
    for (GLint sourceFormat : sourceFormats)
    {
        for (GLint destFormat : destFormats)
        {
            glBindTexture(GL_TEXTURE_2D, mTextures[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, sourceFormat, 1, 1, 0, sourceFormat, GL_UNSIGNED_BYTE,
                         nullptr);
            EXPECT_GL_NO_ERROR();

            glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, destFormat,
                                  GL_UNSIGNED_BYTE, false, false, false);

            EXPECT_GL_NO_ERROR();
        }
    }

    // Test with glCopySubTexture
    for (GLint sourceFormat : sourceFormats)
    {
        for (GLint destFormat : destFormats)
        {
            glBindTexture(GL_TEXTURE_2D, mTextures[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, sourceFormat, 1, 1, 0, sourceFormat, GL_UNSIGNED_BYTE,
                         nullptr);
            EXPECT_GL_NO_ERROR();

            glBindTexture(GL_TEXTURE_2D, mTextures[1]);
            glTexImage2D(GL_TEXTURE_2D, 0, destFormat, 1, 1, 0, destFormat, GL_UNSIGNED_BYTE,
                         nullptr);
            EXPECT_GL_NO_ERROR();

            glCopySubTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, 0, 0, 0, 0, 1,
                                     1, false, false, false);

            EXPECT_GL_NO_ERROR();
        }
    }
}

// Test to ensure that the destination texture is redefined if the properties are different.
TEST_P(CopyTextureTest, RedefineDestinationTexture)
{
    ANGLE_SKIP_TEST_IF(!checkExtensions());
    ANGLE_SKIP_TEST_IF(!extensionEnabled("GL_EXT_texture_format_BGRA8888"));

    GLColor pixels[4] = {GLColor::red, GLColor::red, GLColor::red, GLColor::red};

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, 1, 1, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);
    EXPECT_GL_NO_ERROR();

    // GL_INVALID_OPERATION due to "intrinsic format" != "internal format".
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    // GL_INVALID_VALUE due to bad dimensions.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // If the dest texture has different properties, glCopyTextureCHROMIUM()
    // redefines them.
    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);
    EXPECT_GL_NO_ERROR();

    // glTexSubImage2D() succeeds because mTextures[1] is redefined into 2x2
    // dimension and GL_RGBA format.
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    EXPECT_GL_NO_ERROR();

    // Check that FB is complete.
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    EXPECT_PIXEL_COLOR_EQ(1, 1, pixels[3]);
    EXPECT_GL_NO_ERROR();
}

// Test that invalid dimensions in CopySubTexture are validated
TEST_P(CopyTextureTest, CopySubTextureDimension)
{
    if (!checkExtensions())
    {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glCopySubTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, 1, 1, 0, 0, 1, 1,
                             false, false, false);
    EXPECT_GL_NO_ERROR();

    // xoffset < 0
    glCopySubTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, -1, 1, 0, 0, 1, 1,
                             false, false, false);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // x < 0
    glCopySubTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, 1, 1, -1, 0, 1, 1,
                             false, false, false);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // xoffset + width > dest_width
    glCopySubTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, 2, 2, 0, 0, 2, 2,
                             false, false, false);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // x + width > source_width
    glCopySubTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, 0, 0, 1, 1, 2, 2,
                             false, false, false);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that invalid IDs in CopyTexture are validated
TEST_P(CopyTextureTest, CopyTextureInvalidTextureIds)
{
    if (!checkExtensions())
    {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, 99993, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                          false, false, false);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glCopyTextureCHROMIUM(99994, 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA, GL_UNSIGNED_BYTE,
                          false, false, false);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glCopyTextureCHROMIUM(99995, 0, GL_TEXTURE_2D, 99996, 0, GL_RGBA, GL_UNSIGNED_BYTE, false,
                          false, false);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);
    EXPECT_GL_NO_ERROR();
}

// Test that invalid IDs in CopySubTexture are validated
TEST_P(CopyTextureTest, CopySubTextureInvalidTextureIds)
{
    if (!checkExtensions())
    {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glCopySubTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, 99993, 0, 1, 1, 0, 0, 1, 1, false,
                             false, false);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glCopySubTextureCHROMIUM(99994, 0, GL_TEXTURE_2D, mTextures[1], 0, 1, 1, 0, 0, 1, 1, false,
                             false, false);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glCopySubTextureCHROMIUM(99995, 0, GL_TEXTURE_2D, 99996, 0, 1, 1, 0, 0, 1, 1, false, false,
                             false);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glCopySubTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, 1, 1, 0, 0, 1, 1,
                             false, false, false);
    EXPECT_GL_NO_ERROR();
}

TEST_P(CopyTextureTest, InvalidTarget)
{
    ANGLE_SKIP_TEST_IF(!checkExtensions());

    GLTexture textures[2];

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Invalid enum for a completely invalid target
    glCopySubTextureCHROMIUM(textures[0], 0, GL_INVALID_VALUE, textures[1], 0, 1, 1, 0, 0, 1, 1,
                             false, false, false);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Invalid value for a valid target enum but is not valid for the destination texture
    glCopySubTextureCHROMIUM(textures[0], 0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, textures[1], 0, 1, 1,
                             0, 0, 1, 1, false, false, false);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that using an offset in CopySubTexture works correctly
TEST_P(CopyTextureTest, CopySubTextureOffset)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor rgbaPixels[4 * 4] = {GLColor::red, GLColor::green, GLColor::blue, GLColor::black};
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels);

    GLColor transparentPixels[4 * 4] = {GLColor::transparentBlack, GLColor::transparentBlack,
                                        GLColor::transparentBlack, GLColor::transparentBlack};
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, transparentPixels);

    glCopySubTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, 1, 1, 0, 0, 1, 1,
                             false, false, false);
    EXPECT_GL_NO_ERROR();
    glCopySubTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, 1, 0, 1, 0, 1, 1,
                             false, false, false);
    EXPECT_GL_NO_ERROR();
    glCopySubTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, 0, 1, 0, 1, 1, 1,
                             false, false, false);
    EXPECT_GL_NO_ERROR();

    // Check that FB is complete.
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);
    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(1, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, 1, GLColor::blue);
    EXPECT_GL_NO_ERROR();
}

// Test that flipping the Y component works correctly
TEST_P(CopyTextureTest, FlipY)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor rgbaPixels[4] = {GLColor(255u, 255u, 255u, 255u), GLColor(127u, 127u, 127u, 127u),
                             GLColor(63u, 63u, 63u, 127u), GLColor(255u, 255u, 255u, 0u)};

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels);

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, GL_TRUE, GL_FALSE, GL_FALSE);
    EXPECT_GL_NO_ERROR();

    // Check that FB is complete.
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    EXPECT_PIXEL_COLOR_EQ(0, 0, rgbaPixels[2]);
    EXPECT_PIXEL_COLOR_EQ(1, 0, rgbaPixels[3]);
    EXPECT_PIXEL_COLOR_EQ(0, 1, rgbaPixels[0]);
    EXPECT_PIXEL_COLOR_EQ(1, 1, rgbaPixels[1]);
    EXPECT_GL_NO_ERROR();
}

// Test that premultipying the alpha on copy works correctly
TEST_P(CopyTextureTest, PremultiplyAlpha)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor rgbaPixels[4] = {GLColor(255u, 255u, 255u, 255u), GLColor(255u, 255u, 255u, 127u),
                             GLColor(127u, 127u, 127u, 127u), GLColor(255u, 255u, 255u, 0u)};

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels);

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, GL_FALSE, GL_TRUE, GL_FALSE);
    EXPECT_GL_NO_ERROR();

    // Check that FB is complete.
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(255, 255, 255, 255), 1.0);
    EXPECT_PIXEL_COLOR_NEAR(1, 0, GLColor(127, 127, 127, 127), 1.0);
    EXPECT_PIXEL_COLOR_NEAR(0, 1, GLColor(63, 63, 63, 127), 1.0);
    EXPECT_PIXEL_COLOR_NEAR(1, 1, GLColor(0, 0, 0, 0), 1.0);
    EXPECT_GL_NO_ERROR();
}

// Test that unmultipying the alpha on copy works correctly
TEST_P(CopyTextureTest, UnmultiplyAlpha)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor rgbaPixels[4] = {GLColor(255u, 255u, 255u, 255u), GLColor(127u, 127u, 127u, 127u),
                             GLColor(63u, 63u, 63u, 127u), GLColor(255u, 255u, 255u, 0u)};

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels);

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, GL_FALSE, GL_FALSE, GL_TRUE);
    EXPECT_GL_NO_ERROR();

    // Check that FB is complete.
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(255, 255, 255, 255), 1.0);
    EXPECT_PIXEL_COLOR_NEAR(1, 0, GLColor(255, 255, 255, 127), 1.0);
    EXPECT_PIXEL_COLOR_NEAR(0, 1, GLColor(127, 127, 127, 127), 1.0);
    EXPECT_PIXEL_COLOR_NEAR(1, 1, GLColor(255, 255, 255, 0), 1.0);
    EXPECT_GL_NO_ERROR();
}

// Test that unmultipying and premultiplying the alpha is the same as doing neither
TEST_P(CopyTextureTest, UnmultiplyAndPremultiplyAlpha)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor rgbaPixels[4] = {GLColor(255u, 255u, 255u, 255u), GLColor(127u, 127u, 127u, 127u),
                             GLColor(63u, 63u, 63u, 127u), GLColor(255u, 255u, 255u, 0u)};

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels);

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, GL_FALSE, GL_TRUE, GL_TRUE);
    EXPECT_GL_NO_ERROR();

    // Check that FB is complete.
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(255, 255, 255, 255), 1.0);
    EXPECT_PIXEL_COLOR_NEAR(1, 0, GLColor(127, 127, 127, 127), 1.0);
    EXPECT_PIXEL_COLOR_NEAR(0, 1, GLColor(63, 63, 63, 127), 1.0);
    EXPECT_PIXEL_COLOR_NEAR(1, 1, GLColor(255, 255, 255, 0), 1.0);
    EXPECT_GL_NO_ERROR();
}

// Test to ensure that CopyTexture works with LUMINANCE_ALPHA texture
TEST_P(CopyTextureTest, LuminanceAlpha)
{
    if (!checkExtensions())
    {
        return;
    }

    uint8_t originalPixels[] = {163u, 67u};
    GLColor expectedPixels(163u, 163u, 163u, 67u);

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 1, 1, 0, GL_LUMINANCE_ALPHA,
                 GL_UNSIGNED_BYTE, &originalPixels);

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedPixels);
}

// Test to ensure that CopyTexture works with LUMINANCE texture
TEST_P(CopyTextureTest, Luminance)
{
    if (!checkExtensions())
    {
        return;
    }

    uint8_t originalPixels[] = {57u};
    GLColor expectedPixels(57u, 57u, 57u, 255u);

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                 &originalPixels);

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedPixels);
}

// Test to ensure that CopyTexture works with ALPHA texture
TEST_P(CopyTextureTest, Alpha)
{
    if (!checkExtensions())
    {
        return;
    }

    uint8_t originalPixels[] = {77u};
    GLColor expectedPixels(0u, 0u, 0u, 77u);

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 1, 1, 0, GL_ALPHA, GL_UNSIGNED_BYTE, &originalPixels);

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedPixels);
}

// Test that copying to cube maps works
TEST_P(CopyTextureTest, CubeMapTarget)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor pixels = GLColor::red;

    GLTexture textures[2];

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixels);

    glBindTexture(GL_TEXTURE_CUBE_MAP, textures[1]);
    for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
         face++)
    {
        glTexImage2D(face, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    glCopySubTextureCHROMIUM(textures[0], 0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, textures[1], 0, 0, 0,
                             0, 0, 1, 1, false, false, false);

    EXPECT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                           textures[1], 0);

    // Check that FB is complete.
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    EXPECT_PIXEL_COLOR_EQ(0, 0, pixels);

    EXPECT_GL_NO_ERROR();
}

// Test that copying to non-zero mipmaps works
TEST_P(CopyTextureTest, CopyToMipmap)
{
    if (!checkExtensions())
    {
        return;
    }

    if (getClientMajorVersion() < 3 && !extensionEnabled("GL_OES_fbo_render_mipmap"))
    {
        std::cout << "Test skipped because ES3 or GL_OES_fbo_render_mipmap is missing."
                  << std::endl;
        return;
    }

    if (IsOSX() && IsIntel())
    {
        std::cout << "Test skipped on Mac Intel." << std::endl;
        return;
    }

    GLColor pixels[] = {GLColor::red, GLColor::red, GLColor::red, GLColor::red};

    GLTexture textures[2];

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    std::vector<std::pair<GLint, GLint>> soureDestPairs;
    soureDestPairs.push_back(std::make_pair(0, 1));

    // ES3 allows copying from non-zero mips
    if (getClientMajorVersion() >= 3)
    {
        soureDestPairs.push_back(std::make_pair(1, 2));
    }

    for (const auto &sourceDestPair : soureDestPairs)
    {
        const GLint sourceLevel = sourceDestPair.first;
        const GLint destLevel   = sourceDestPair.second;

        glCopyTextureCHROMIUM(textures[0], sourceLevel, GL_TEXTURE_2D, textures[1], destLevel,
                              GL_RGBA, GL_UNSIGNED_BYTE, false, false, false);

        EXPECT_GL_NO_ERROR();

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[1],
                               destLevel);

        // Check that FB is complete.
        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

        EXPECT_PIXEL_COLOR_EQ(0, 0, pixels[0]);

        EXPECT_GL_NO_ERROR();
    }
}

// Test to ensure that CopyTexture works with LUMINANCE texture as a destination
TEST_P(CopyTextureTestDest, Luminance)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor originalPixels(50u, 100u, 150u, 200u);
    GLColor expectedPixels(50u, 50u, 50u, 255u);

    // ReadPixels doesn't work with LUMINANCE (non-renderable), so we copy again back to an RGBA
    // texture to verify contents.
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &originalPixels);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);

    glCopyTextureCHROMIUM(mTextures[1], 0, GL_TEXTURE_2D, mTextures[0], 0, GL_LUMINANCE,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedPixels);
}

// Test to ensure that CopyTexture works with LUMINANCE texture as a destination with
// UnpackPremultiply parameter
TEST_P(CopyTextureTestDest, LuminanceMultiply)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor originalPixels(50u, 100u, 150u, 200u);
    GLColor expectedPixels(39u, 39u, 39u, 255u);

    // ReadPixels doesn't work with LUMINANCE (non-renderable), so we copy again back to an RGBA
    // texture to verify contents.
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &originalPixels);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);

    glCopyTextureCHROMIUM(mTextures[1], 0, GL_TEXTURE_2D, mTextures[0], 0, GL_LUMINANCE,
                          GL_UNSIGNED_BYTE, false, true, false);

    EXPECT_GL_NO_ERROR();

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedPixels);
}

// Test to ensure that CopyTexture works with LUMINANCE texture as a destination with
// UnpackUnmultiply parameter
TEST_P(CopyTextureTestDest, LuminanceUnmultiply)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor originalPixels(50u, 100u, 150u, 200u);
    GLColor expectedPixels(64u, 64u, 64u, 255u);

    // ReadPixels doesn't work with LUMINANCE (non-renderable), so we copy again back to an RGBA
    // texture to verify contents.
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &originalPixels);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);

    glCopyTextureCHROMIUM(mTextures[1], 0, GL_TEXTURE_2D, mTextures[0], 0, GL_LUMINANCE,
                          GL_UNSIGNED_BYTE, false, false, true);

    EXPECT_GL_NO_ERROR();

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedPixels);
}

// Test to ensure that CopyTexture works with LUMINANCE_ALPHA texture as a destination
TEST_P(CopyTextureTestDest, LuminanceAlpha)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor originalPixels(50u, 100u, 150u, 200u);
    GLColor expectedPixels(50u, 50u, 50u, 200u);

    // ReadPixels doesn't work with LUMINANCE_ALPHA (non-renderable), so we copy again back to an
    // RGBA texture to verify contents.
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &originalPixels);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 1, 1, 0, GL_LUMINANCE_ALPHA,
                 GL_UNSIGNED_BYTE, nullptr);

    glCopyTextureCHROMIUM(mTextures[1], 0, GL_TEXTURE_2D, mTextures[0], 0, GL_LUMINANCE_ALPHA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedPixels);
}

// Test to ensure that CopyTexture works with LUMINANCE_ALPHA texture as a destination with
// UnpackPremultiply parameter
TEST_P(CopyTextureTestDest, LuminanceAlphaMultiply)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor originalPixels(50u, 100u, 150u, 200u);
    GLColor expectedPixels(39u, 39u, 39u, 200u);

    // ReadPixels doesn't work with LUMINANCE_ALPHA (non-renderable), so we copy again back to an
    // RGBA texture to verify contents.
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &originalPixels);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 1, 1, 0, GL_LUMINANCE_ALPHA,
                 GL_UNSIGNED_BYTE, nullptr);

    glCopyTextureCHROMIUM(mTextures[1], 0, GL_TEXTURE_2D, mTextures[0], 0, GL_LUMINANCE_ALPHA,
                          GL_UNSIGNED_BYTE, false, true, false);

    EXPECT_GL_NO_ERROR();

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedPixels);
}

// Test to ensure that CopyTexture works with LUMINANCE_ALPHA texture as a destination with
// UnpackUnmultiplyAlpha parameter
TEST_P(CopyTextureTestDest, LuminanceAlphaUnmultiply)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor originalPixels(50u, 100u, 150u, 200u);
    GLColor expectedPixels(64u, 64u, 64u, 200u);

    // ReadPixels doesn't work with LUMINANCE_ALPHA (non-renderable), so we copy again back to an
    // RGBA texture to verify contents.
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &originalPixels);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 1, 1, 0, GL_LUMINANCE_ALPHA,
                 GL_UNSIGNED_BYTE, nullptr);

    glCopyTextureCHROMIUM(mTextures[1], 0, GL_TEXTURE_2D, mTextures[0], 0, GL_LUMINANCE_ALPHA,
                          GL_UNSIGNED_BYTE, false, false, true);

    EXPECT_GL_NO_ERROR();

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedPixels);
}

// Test to ensure that CopyTexture works with ALPHA texture as a destination
TEST_P(CopyTextureTestDest, Alpha)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor originalPixels(50u, 100u, 150u, 155u);
    GLColor expectedPixels(0u, 0u, 0u, 155u);

    // ReadPixels doesn't work with ALPHA (non-renderable), so we copy again back to an RGBA
    // texture to verify contents.
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &originalPixels);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 1, 1, 0, GL_ALPHA, GL_UNSIGNED_BYTE, nullptr);

    glCopyTextureCHROMIUM(mTextures[1], 0, GL_TEXTURE_2D, mTextures[0], 0, GL_ALPHA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedPixels);
}

// Test to ensure that CopyTexture works with ALPHA texture as a destination with
// UnpackPremultiplyAlpha parameter
TEST_P(CopyTextureTestDest, AlphaMultiply)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor originalPixels(50u, 100u, 150u, 155u);
    GLColor expectedPixels(0u, 0u, 0u, 155u);

    // ReadPixels doesn't work with ALPHA (non-renderable), so we copy again back to an RGBA
    // texture to verify contents.
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &originalPixels);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 1, 1, 0, GL_ALPHA, GL_UNSIGNED_BYTE, nullptr);

    glCopyTextureCHROMIUM(mTextures[1], 0, GL_TEXTURE_2D, mTextures[0], 0, GL_ALPHA,
                          GL_UNSIGNED_BYTE, false, true, false);

    EXPECT_GL_NO_ERROR();

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedPixels);
}

// Test to ensure that CopyTexture works with ALPHA texture as a destination with
// UnpackUnmultiplyAlpha parameter
TEST_P(CopyTextureTestDest, AlphaUnmultiply)
{
    if (!checkExtensions())
    {
        return;
    }

    GLColor originalPixels(50u, 100u, 150u, 155u);
    GLColor expectedPixels(0u, 0u, 0u, 155u);

    // ReadPixels doesn't work with ALPHA (non-renderable), so we copy again back to an RGBA
    // texture to verify contents.
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &originalPixels);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 1, 1, 0, GL_ALPHA, GL_UNSIGNED_BYTE, nullptr);

    glCopyTextureCHROMIUM(mTextures[1], 0, GL_TEXTURE_2D, mTextures[0], 0, GL_ALPHA,
                          GL_UNSIGNED_BYTE, false, false, true);

    EXPECT_GL_NO_ERROR();

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedPixels);
}

// Test to ensure that CopyTexture uses the correct ALPHA passthrough shader to ensure RGB channels
// are set to 0.
TEST_P(CopyTextureTestDest, AlphaCopyWithRGB)
{
    ANGLE_SKIP_TEST_IF(!checkExtensions());

    GLColor originalPixels(50u, 100u, 150u, 155u);
    GLColor expectedPixels(0u, 0u, 0u, 155u);

    // ReadPixels doesn't work with ALPHA (non-renderable), so we copy again back to an RGBA
    // texture to verify contents.
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &originalPixels);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 1, 1, 0, GL_ALPHA, GL_HALF_FLOAT_OES, nullptr);

    glCopyTextureCHROMIUM(mTextures[1], 0, GL_TEXTURE_2D, mTextures[0], 0, GL_ALPHA,
                          GL_HALF_FLOAT_OES, false, false, false);

    EXPECT_GL_NO_ERROR();

    glCopyTextureCHROMIUM(mTextures[0], 0, GL_TEXTURE_2D, mTextures[1], 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);

    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedPixels);
}

// Test the newly added ES3 unorm formats
TEST_P(CopyTextureTestES3, ES3UnormFormats)
{
    if (!checkExtensions())
    {
        return;
    }

    auto testOutput = [this](GLuint texture, const GLColor &expectedColor) {
        const std::string vs =
            "#version 300 es\n"
            "in vec4 position;\n"
            "out vec2 texcoord;\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
            "    texcoord = (position.xy * 0.5) + 0.5;\n"
            "}\n";

        const std::string fs =
            "#version 300 es\n"
            "precision mediump float;\n"
            "uniform sampler2D tex;\n"
            "in vec2 texcoord;\n"
            "out vec4 color;\n"
            "void main()\n"
            "{\n"
            "    color = texture(tex, texcoord);\n"
            "}\n";

        ANGLE_GL_PROGRAM(program, vs, fs);
        glUseProgram(program);

        GLRenderbuffer rbo;
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glUniform1i(glGetUniformLocation(program.get(), "tex"), 0);

        drawQuad(program, "position", 0.5f, 1.0f, true);

        EXPECT_PIXEL_COLOR_NEAR(0, 0, expectedColor, 1.0);
    };

    auto testCopyCombination = [this, testOutput](GLenum sourceInternalFormat, GLenum sourceFormat,
                                                  GLenum sourceType, const GLColor &sourceColor,
                                                  GLenum destInternalFormat, GLenum destType,
                                                  bool flipY, bool premultiplyAlpha,
                                                  bool unmultiplyAlpha,
                                                  const GLColor &expectedColor) {

        GLTexture sourceTexture;
        glBindTexture(GL_TEXTURE_2D, sourceTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, sourceInternalFormat, 1, 1, 0, sourceFormat, sourceType,
                     &sourceColor);

        GLTexture destTexture;
        glBindTexture(GL_TEXTURE_2D, destTexture);

        glCopyTextureCHROMIUM(sourceTexture, 0, GL_TEXTURE_2D, destTexture, 0, destInternalFormat,
                              destType, flipY, premultiplyAlpha, unmultiplyAlpha);
        ASSERT_GL_NO_ERROR();

        testOutput(destTexture, expectedColor);
    };

    // New LUMA source formats
    testCopyCombination(GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE, GLColor(128, 0, 0, 0), GL_RGB,
                        GL_UNSIGNED_BYTE, false, false, false, GLColor(128, 128, 128, 255));
    testCopyCombination(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE,
                        GLColor(128, 64, 0, 0), GL_RGB, GL_UNSIGNED_BYTE, false, false, false,
                        GLColor(128, 128, 128, 255));
    testCopyCombination(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE,
                        GLColor(128, 64, 0, 0), GL_RGB, GL_UNSIGNED_BYTE, false, true, false,
                        GLColor(32, 32, 32, 255));
    testCopyCombination(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE,
                        GLColor(128, 128, 0, 0), GL_RGB, GL_UNSIGNED_BYTE, false, false, true,
                        GLColor(255, 255, 255, 255));
    testCopyCombination(GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE, GLColor(128, 0, 0, 0), GL_RGBA,
                        GL_UNSIGNED_BYTE, false, false, false, GLColor(0, 0, 0, 128));
    testCopyCombination(GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE, GLColor(128, 0, 0, 0), GL_RGBA,
                        GL_UNSIGNED_BYTE, false, false, true, GLColor(0, 0, 0, 128));
    testCopyCombination(GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE, GLColor(128, 0, 0, 0), GL_RGBA,
                        GL_UNSIGNED_BYTE, false, true, false, GLColor(0, 0, 0, 128));

    // New sRGB dest formats
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_SRGB,
                        GL_UNSIGNED_BYTE, false, false, false, GLColor(55, 13, 4, 255));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_SRGB,
                        GL_UNSIGNED_BYTE, false, true, false, GLColor(13, 4, 1, 255));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128),
                        GL_SRGB_ALPHA_EXT, GL_UNSIGNED_BYTE, false, false, false,
                        GLColor(55, 13, 4, 128));
}

// Test the newly added ES3 float formats
TEST_P(CopyTextureTestES3, ES3FloatFormats)
{
    ANGLE_SKIP_TEST_IF(IsIntel() && IsWindows() && IsOpenGL());

    if (!checkExtensions())
    {
        return;
    }

    if (!extensionEnabled("GL_EXT_color_buffer_float"))
    {
        std::cout << "Test skipped due to missing GL_EXT_color_buffer_float." << std::endl;
        return;
    }

    auto testOutput = [this](GLuint texture, const GLColor32F &expectedColor) {
        const std::string vs =
            "#version 300 es\n"
            "in vec4 position;\n"
            "out vec2 texcoord;\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
            "    texcoord = (position.xy * 0.5) + 0.5;\n"
            "}\n";

        const std::string fs =
            "#version 300 es\n"
            "precision mediump float;\n"
            "uniform sampler2D tex;\n"
            "in vec2 texcoord;\n"
            "out vec4 color;\n"
            "void main()\n"
            "{\n"
            "    color = texture(tex, texcoord);\n"
            "}\n";

        ANGLE_GL_PROGRAM(program, vs, fs);
        glUseProgram(program);

        GLRenderbuffer rbo;
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, 1, 1);

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glUniform1i(glGetUniformLocation(program.get(), "tex"), 0);

        drawQuad(program, "position", 0.5f, 1.0f, true);

        EXPECT_PIXEL_COLOR32F_NEAR(0, 0, expectedColor, 0.05);
    };

    auto testCopyCombination = [this, testOutput](GLenum sourceInternalFormat, GLenum sourceFormat,
                                                  GLenum sourceType, const GLColor &sourceColor,
                                                  GLenum destInternalFormat, GLenum destType,
                                                  bool flipY, bool premultiplyAlpha,
                                                  bool unmultiplyAlpha,
                                                  const GLColor32F &expectedColor) {

        GLTexture sourceTexture;
        glBindTexture(GL_TEXTURE_2D, sourceTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, sourceInternalFormat, 1, 1, 0, sourceFormat, sourceType,
                     &sourceColor);

        GLTexture destTexture;
        glBindTexture(GL_TEXTURE_2D, destTexture);

        glCopyTextureCHROMIUM(sourceTexture, 0, GL_TEXTURE_2D, destTexture, 0, destInternalFormat,
                              destType, flipY, premultiplyAlpha, unmultiplyAlpha);
        ASSERT_GL_NO_ERROR();

        testOutput(destTexture, expectedColor);
    };

    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGBA32F,
                        GL_FLOAT, false, false, false, GLColor32F(0.5f, 0.25f, 0.125f, 0.5f));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGBA32F,
                        GL_FLOAT, false, true, false, GLColor32F(0.25f, 0.125f, 0.0625f, 0.5f));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGBA32F,
                        GL_FLOAT, false, false, true, GLColor32F(1.0f, 0.5f, 0.25f, 0.5f));

    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_R16F,
                        GL_FLOAT, false, false, false, GLColor32F(0.5f, 0.0f, 0.0f, 1.0f));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_R16F,
                        GL_FLOAT, false, true, false, GLColor32F(0.25f, 0.0f, 0.0f, 1.0f));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_R16F,
                        GL_FLOAT, false, false, true, GLColor32F(1.0f, 0.0f, 0.0f, 1.0f));

    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RG16F,
                        GL_FLOAT, false, false, false, GLColor32F(0.5f, 0.25f, 0.0f, 1.0f));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RG16F,
                        GL_FLOAT, false, true, false, GLColor32F(0.25f, 0.125f, 0.0f, 1.0f));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RG16F,
                        GL_FLOAT, false, false, true, GLColor32F(1.0f, 0.5f, 0.0f, 1.0f));

    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGB16F,
                        GL_FLOAT, false, false, false, GLColor32F(0.5f, 0.25f, 0.125f, 1.0f));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGB16F,
                        GL_FLOAT, false, true, false, GLColor32F(0.25f, 0.125f, 0.0625f, 1.0f));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGB16F,
                        GL_FLOAT, false, false, true, GLColor32F(1.0f, 0.5f, 0.25f, 1.0f));

    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128),
                        GL_R11F_G11F_B10F, GL_FLOAT, false, false, false,
                        GLColor32F(0.5f, 0.25f, 0.125f, 1.0f));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128),
                        GL_R11F_G11F_B10F, GL_FLOAT, false, true, false,
                        GLColor32F(0.25f, 0.125f, 0.0625f, 1.0f));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128),
                        GL_R11F_G11F_B10F, GL_FLOAT, false, false, true,
                        GLColor32F(1.0f, 0.5f, 0.25f, 1.0f));

    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGB9_E5,
                        GL_FLOAT, false, false, false, GLColor32F(0.5f, 0.25f, 0.125f, 1.0f));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGB9_E5,
                        GL_FLOAT, false, true, false, GLColor32F(0.25f, 0.125f, 0.0625f, 1.0f));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGB9_E5,
                        GL_FLOAT, false, false, true, GLColor32F(1.0f, 0.5f, 0.25f, 1.0f));
}

// Test the newly added ES3 unsigned integer formats
TEST_P(CopyTextureTestES3, ES3UintFormats)
{
    ANGLE_SKIP_TEST_IF(IsLinux() && IsOpenGL() && IsIntel());

    if (!checkExtensions())
    {
        return;
    }

    using GLColor32U = std::tuple<GLuint, GLuint, GLuint, GLuint>;

    auto testOutput = [this](GLuint texture, const GLColor32U &expectedColor) {
        const std::string vs =
            "#version 300 es\n"
            "in vec4 position;\n"
            "out vec2 texcoord;\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
            "    texcoord = (position.xy * 0.5) + 0.5;\n"
            "}\n";

        std::string fs =
            "#version 300 es\n"
            "precision mediump float;\n"
            "precision mediump usampler2D;\n"
            "in vec2 texcoord;\n"
            "uniform usampler2D tex;\n"
            "out uvec4 color;\n"
            "void main()\n"
            "{\n"
            "    color = texture(tex, texcoord);\n"
            "}\n";

        ANGLE_GL_PROGRAM(program, vs, fs);
        glUseProgram(program);

        GLRenderbuffer rbo;
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8UI, 1, 1);

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glUniform1i(glGetUniformLocation(program.get(), "tex"), 0);

        drawQuad(program, "position", 0.5f, 1.0f, true);
        ASSERT_GL_NO_ERROR();

        GLuint pixel[4] = {0};
        glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, pixel);
        ASSERT_GL_NO_ERROR();
        EXPECT_NEAR(std::get<0>(expectedColor), pixel[0], 1);
        EXPECT_NEAR(std::get<1>(expectedColor), pixel[1], 1);
        EXPECT_NEAR(std::get<2>(expectedColor), pixel[2], 1);
        EXPECT_NEAR(std::get<3>(expectedColor), pixel[3], 1);
    };

    auto testCopyCombination = [this, testOutput](GLenum sourceInternalFormat, GLenum sourceFormat,
                                                  GLenum sourceType, const GLColor &sourceColor,
                                                  GLenum destInternalFormat, GLenum destType,
                                                  bool flipY, bool premultiplyAlpha,
                                                  bool unmultiplyAlpha,
                                                  const GLColor32U &expectedColor) {

        GLTexture sourceTexture;
        glBindTexture(GL_TEXTURE_2D, sourceTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, sourceInternalFormat, 1, 1, 0, sourceFormat, sourceType,
                     &sourceColor);

        GLTexture destTexture;
        glBindTexture(GL_TEXTURE_2D, destTexture);

        glCopyTextureCHROMIUM(sourceTexture, 0, GL_TEXTURE_2D, destTexture, 0, destInternalFormat,
                              destType, flipY, premultiplyAlpha, unmultiplyAlpha);
        ASSERT_GL_NO_ERROR();

        testOutput(destTexture, expectedColor);
    };

    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGBA8UI,
                        GL_UNSIGNED_BYTE, false, false, false, GLColor32U(128, 64, 32, 128));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGBA8UI,
                        GL_UNSIGNED_BYTE, false, true, false, GLColor32U(64, 32, 16, 128));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGBA8UI,
                        GL_UNSIGNED_BYTE, false, false, true, GLColor32U(255, 128, 64, 128));

    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGB8UI,
                        GL_UNSIGNED_BYTE, false, false, false, GLColor32U(128, 64, 32, 1));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGB8UI,
                        GL_UNSIGNED_BYTE, false, true, false, GLColor32U(64, 32, 16, 1));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RGB8UI,
                        GL_UNSIGNED_BYTE, false, false, true, GLColor32U(255, 128, 64, 1));

    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RG8UI,
                        GL_UNSIGNED_BYTE, false, false, false, GLColor32U(128, 64, 0, 1));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RG8UI,
                        GL_UNSIGNED_BYTE, false, true, false, GLColor32U(64, 32, 0, 1));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_RG8UI,
                        GL_UNSIGNED_BYTE, false, false, true, GLColor32U(255, 128, 0, 1));

    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_R8UI,
                        GL_UNSIGNED_BYTE, false, false, false, GLColor32U(128, 0, 0, 1));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(128, 64, 32, 128), GL_R8UI,
                        GL_UNSIGNED_BYTE, false, true, false, GLColor32U(64, 0, 0, 1));
    testCopyCombination(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GLColor(120, 64, 32, 128), GL_R8UI,
                        GL_UNSIGNED_BYTE, false, false, true, GLColor32U(240, 0, 0, 1));
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(CopyTextureTest, ES2_D3D9(), ES2_D3D11(), ES2_OPENGL(), ES2_OPENGLES());
ANGLE_INSTANTIATE_TEST(CopyTextureTestDest, ES2_D3D11());
ANGLE_INSTANTIATE_TEST(CopyTextureTestES3, ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES());

}  // namespace angle
