//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GetImageTest:
//   Tests for the ANGLE_get_image extension.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{
constexpr uint32_t kSize        = 32;
constexpr char kExtensionName[] = "GL_ANGLE_get_image";
constexpr uint32_t kSmallSize   = 2;

class GetImageTest : public ANGLETest
{
  public:
    GetImageTest()
    {
        setWindowWidth(kSize);
        setWindowHeight(kSize);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

class GetImageTestNoExtensions : public ANGLETest
{
  public:
    GetImageTestNoExtensions() { setExtensionsEnabled(false); }
};

GLTexture InitTextureWithSize(uint32_t size, void *pixelData)
{
    // Create a simple texture.
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

GLTexture InitSimpleTexture()
{
    std::vector<GLColor> pixelData(kSize * kSize, GLColor::red);
    return InitTextureWithSize(kSize, pixelData.data());
}

GLRenderbuffer InitRenderbufferWithSize(uint32_t size)
{
    // Create a simple renderbuffer.
    GLRenderbuffer renderbuf;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuf);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, size, size);
    return renderbuf;
}

GLRenderbuffer InitSimpleRenderbuffer()
{
    return InitRenderbufferWithSize(kSize);
}

// Test validation for the extension functions.
TEST_P(GetImageTest, NegativeAPI)
{
    // Verify the extension is enabled.
    ASSERT_TRUE(IsGLExtensionEnabled(kExtensionName));

    // Draw once with simple texture.
    GLTexture tex = InitSimpleTexture();
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Texture2D(), essl1_shaders::fs::Texture2D());
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    ASSERT_GL_NO_ERROR();

    // Pack pixels tightly.
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    // Verify GetTexImage can work with correct parameters.
    std::vector<GLColor> buffer(kSize * kSize);
    glGetTexImageANGLE(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
    EXPECT_GL_NO_ERROR();

    // Test invalid texture target.
    glGetTexImageANGLE(GL_TEXTURE_CUBE_MAP, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Test invalid texture level.
    glGetTexImageANGLE(GL_TEXTURE_2D, -1, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glGetTexImageANGLE(GL_TEXTURE_2D, 2000, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Test invalid format and type.
    glGetTexImageANGLE(GL_TEXTURE_2D, 0, GL_NONE, GL_UNSIGNED_BYTE, buffer.data());
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glGetTexImageANGLE(GL_TEXTURE_2D, 0, GL_RGBA, GL_NONE, buffer.data());
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Create a simple renderbuffer.
    GLRenderbuffer renderbuf = InitSimpleRenderbuffer();
    ASSERT_GL_NO_ERROR();

    // Verify GetRenderbufferImage can work with correct parameters.
    glGetRenderbufferImageANGLE(GL_RENDERBUFFER, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
    EXPECT_GL_NO_ERROR();

    // Test invalid renderbuffer target.
    glGetRenderbufferImageANGLE(GL_TEXTURE_2D, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Test invalid renderbuffer format/type.
    glGetRenderbufferImageANGLE(GL_RENDERBUFFER, GL_NONE, GL_UNSIGNED_BYTE, buffer.data());
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glGetRenderbufferImageANGLE(GL_RENDERBUFFER, GL_RGBA, GL_NONE, buffer.data());
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Pack buffer tests. Requires ES 3+ or extension.
    if (getClientMajorVersion() >= 3 || IsGLExtensionEnabled("GL_NV_pixel_buffer_object"))
    {
        // Test valid pack buffer.
        GLBuffer packBuffer;
        glBindBuffer(GL_PIXEL_PACK_BUFFER, packBuffer);
        glBufferData(GL_PIXEL_PACK_BUFFER, kSize * kSize * sizeof(GLColor), nullptr,
                     GL_STATIC_DRAW);
        glGetTexImageANGLE(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
        glGetRenderbufferImageANGLE(GL_RENDERBUFFER, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        // Test too small pack buffer.
        glBufferData(GL_PIXEL_PACK_BUFFER, kSize, nullptr, GL_STATIC_DRAW);
        glGetTexImageANGLE(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
        glGetRenderbufferImageANGLE(GL_RENDERBUFFER, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
}

// Simple test for GetTexImage
TEST_P(GetImageTest, GetTexImage)
{
    // Verify the extension is enabled.
    ASSERT_TRUE(IsGLExtensionEnabled(kExtensionName));

    constexpr uint32_t kSmallSize     = 2;
    std::vector<GLColor> expectedData = {GLColor::red, GLColor::blue, GLColor::green,
                                         GLColor::yellow};

    glViewport(0, 0, kSmallSize, kSmallSize);

    // Draw once with simple texture.
    GLTexture tex = InitTextureWithSize(kSmallSize, expectedData.data());
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Texture2D(), essl1_shaders::fs::Texture2D());
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    ASSERT_GL_NO_ERROR();

    // Pack pixels tightly.
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    // Verify GetImage.
    std::vector<GLColor> actualData(kSmallSize * kSmallSize);
    glGetTexImageANGLE(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, actualData.data());
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(expectedData, actualData);
}

// Simple cube map test for GetTexImage
TEST_P(GetImageTest, CubeMap)
{
    // Verify the extension is enabled.
    ASSERT_TRUE(IsGLExtensionEnabled(kExtensionName));

    const std::array<std::array<GLColor, kSmallSize * kSmallSize>, kCubeFaces.size()> expectedData =
        {{
            {GLColor::red, GLColor::red, GLColor::red, GLColor::red},
            {GLColor::green, GLColor::green, GLColor::green, GLColor::green},
            {GLColor::blue, GLColor::blue, GLColor::blue, GLColor::blue},
            {GLColor::yellow, GLColor::yellow, GLColor::yellow, GLColor::yellow},
            {GLColor::cyan, GLColor::cyan, GLColor::cyan, GLColor::cyan},
            {GLColor::magenta, GLColor::magenta, GLColor::magenta, GLColor::magenta},
        }};

    GLTexture texture;
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

    for (size_t faceIndex = 0; faceIndex < kCubeFaces.size(); ++faceIndex)
    {
        glTexImage2D(kCubeFaces[faceIndex], 0, GL_RGBA, kSmallSize, kSmallSize, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, expectedData[faceIndex].data());
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Pack pixels tightly.
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    // Verify GetImage.
    std::array<GLColor, kSmallSize *kSmallSize> actualData = {};
    for (size_t faceIndex = 0; faceIndex < kCubeFaces.size(); ++faceIndex)
    {
        glGetTexImageANGLE(kCubeFaces[faceIndex], 0, GL_RGBA, GL_UNSIGNED_BYTE, actualData.data());
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(expectedData[faceIndex], actualData);
    }
}

// Simple test for GetRenderbufferImage
TEST_P(GetImageTest, GetRenderbufferImage)
{
    // Verify the extension is enabled.
    ASSERT_TRUE(IsGLExtensionEnabled(kExtensionName));

    std::vector<GLColor> expectedData = {GLColor::red, GLColor::blue, GLColor::green,
                                         GLColor::yellow};

    glViewport(0, 0, kSmallSize, kSmallSize);

    // Set up a simple Framebuffer with a Renderbuffer.
    GLRenderbuffer renderbuffer = InitRenderbufferWithSize(kSmallSize);
    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Draw once with simple texture.
    GLTexture tex = InitTextureWithSize(kSmallSize, expectedData.data());
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Texture2D(), essl1_shaders::fs::Texture2D());
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    ASSERT_GL_NO_ERROR();

    // Pack pixels tightly.
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    // Verify GetImage.
    std::vector<GLColor> actualData(kSmallSize * kSmallSize);
    glGetRenderbufferImageANGLE(GL_RENDERBUFFER, GL_RGBA, GL_UNSIGNED_BYTE, actualData.data());
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(expectedData, actualData);
}

// Verifies that the extension enums and entry points are invalid when the extension is disabled.
TEST_P(GetImageTestNoExtensions, EntryPointsInactive)
{
    // Verify the extension is not enabled.
    ASSERT_FALSE(IsGLExtensionEnabled(kExtensionName));

    // Draw once with simple texture.
    GLTexture tex = InitSimpleTexture();
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Texture2D(), essl1_shaders::fs::Texture2D());
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    ASSERT_GL_NO_ERROR();

    // Query implementation format and type. Should give invalid enum.
    GLint param;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_IMPLEMENTATION_COLOR_READ_FORMAT, &param);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_IMPLEMENTATION_COLOR_READ_TYPE, &param);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Verify calling GetTexImage produces an error.
    std::vector<GLColor> buffer(kSize * kSize, 0);
    glGetTexImageANGLE(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Create a simple renderbuffer.
    GLRenderbuffer renderbuf = InitSimpleRenderbuffer();
    ASSERT_GL_NO_ERROR();

    // Query implementation format and type. Should give invalid enum.
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_IMPLEMENTATION_COLOR_READ_FORMAT, &param);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_IMPLEMENTATION_COLOR_READ_TYPE, &param);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Verify calling GetRenderbufferImage produces an error.
    glGetRenderbufferImageANGLE(GL_RENDERBUFFER, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

ANGLE_INSTANTIATE_TEST(GetImageTest, ES2_VULKAN(), ES3_VULKAN());
ANGLE_INSTANTIATE_TEST(GetImageTestNoExtensions, ES2_VULKAN(), ES3_VULKAN());
}  // namespace