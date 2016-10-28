//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

using namespace angle;

namespace
{

void TexImageCubeMapFaces(GLint level,
                          GLenum internalformat,
                          GLsizei width,
                          GLenum format,
                          GLenum type,
                          void *pixels)
{
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, level, internalformat, width, width, 0, format,
                 type, pixels);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, level, internalformat, width, width, 0, format,
                 type, pixels);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, level, internalformat, width, width, 0, format,
                 type, pixels);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, level, internalformat, width, width, 0, format,
                 type, pixels);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, level, internalformat, width, width, 0, format,
                 type, pixels);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, level, internalformat, width, width, 0, format,
                 type, pixels);
}

class BaseMipmapTest : public ANGLETest
{
  protected:
    void clearAndDrawQuad(GLuint program, GLsizei viewportWidth, GLsizei viewportHeight)
    {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0, 0, viewportWidth, viewportHeight);
        ASSERT_GL_NO_ERROR();

        drawQuad(program, "position", 0.0f);
    }
};

}  // namespace

class MipmapTest : public BaseMipmapTest
{
  protected:
    MipmapTest()
        : m2DProgram(0),
          mCubeProgram(0),
          mTexture2D(0),
          mTextureCube(0),
          mLevelZeroBlueInitData(nullptr),
          mLevelZeroWhiteInitData(nullptr),
          mLevelOneInitData(nullptr),
          mLevelTwoInitData(nullptr),
          mOffscreenFramebuffer(0)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void setUp2DProgram()
    {
        // Vertex Shader source
        // clang-format off
        const std::string vs = SHADER_SOURCE
        (
            attribute vec4 position;
            varying vec2 vTexCoord;

            void main()
            {
                gl_Position = position;
                vTexCoord   = (position.xy * 0.5) + 0.5;
            }
        );

        // Fragment Shader source
        const std::string fs = SHADER_SOURCE
        (
            precision mediump float;

            uniform sampler2D uTexture;
            varying vec2 vTexCoord;

            void main()
            {
                gl_FragColor = texture2D(uTexture, vTexCoord);
            }
        );
        // clang-format on

        m2DProgram = CompileProgram(vs, fs);
        ASSERT_NE(0u, m2DProgram);
    }

    void setUpCubeProgram()
    {
        // A simple vertex shader for the texture cube
        // clang-format off
        const std::string cubeVS = SHADER_SOURCE
        (
            attribute vec4 position;
            varying vec4 vPosition;
            void main()
            {
                gl_Position = position;
                vPosition = position;
            }
        );

        // A very simple fragment shader to sample from the negative-Y face of a texture cube.
        const std::string cubeFS = SHADER_SOURCE
        (
            precision mediump float;
            uniform samplerCube uTexture;
            varying vec4 vPosition;

            void main()
            {
                gl_FragColor = textureCube(uTexture, vec3(vPosition.x, -1, vPosition.y));
            }
        );
        // clang-format on

        mCubeProgram = CompileProgram(cubeVS, cubeFS);
        ASSERT_NE(0u, mCubeProgram);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        setUp2DProgram();

        setUpCubeProgram();

        mLevelZeroBlueInitData = createRGBInitData(getWindowWidth(), getWindowHeight(), 0, 0, 255); // Blue
        mLevelZeroWhiteInitData = createRGBInitData(getWindowWidth(), getWindowHeight(), 255, 255, 255); // White
        mLevelOneInitData = createRGBInitData((getWindowWidth() / 2), (getWindowHeight() / 2), 0, 255, 0);   // Green
        mLevelTwoInitData = createRGBInitData((getWindowWidth() / 4), (getWindowHeight() / 4), 255, 0, 0);   // Red

        glGenFramebuffers(1, &mOffscreenFramebuffer);
        glGenTextures(1, &mTexture2D);

        // Initialize the texture2D to be empty, and don't use mips.
        glBindTexture(GL_TEXTURE_2D, mTexture2D);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, getWindowWidth(), getWindowHeight(), 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        ASSERT_EQ(getWindowWidth(), getWindowHeight());

        // Create a non-mipped texture cube. Set the negative-Y face to be blue.
        glGenTextures(1, &mTextureCube);
        glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureCube);
        TexImageCubeMapFaces(0, GL_RGB, getWindowWidth(), GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, getWindowWidth(), getWindowWidth(),
                     0, GL_RGB, GL_UNSIGNED_BYTE, mLevelZeroBlueInitData);

        // Complete the texture cube without mipmaps to start with.
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        ASSERT_GL_NO_ERROR();
    }

    void TearDown() override
    {
        glDeleteProgram(m2DProgram);
        glDeleteProgram(mCubeProgram);
        glDeleteFramebuffers(1, &mOffscreenFramebuffer);
        glDeleteTextures(1, &mTexture2D);
        glDeleteTextures(1, &mTextureCube);

        SafeDeleteArray(mLevelZeroBlueInitData);
        SafeDeleteArray(mLevelZeroWhiteInitData);
        SafeDeleteArray(mLevelOneInitData);
        SafeDeleteArray(mLevelTwoInitData);

        ANGLETest::TearDown();
    }

    GLubyte *createRGBInitData(GLint width, GLint height, GLint r, GLint g, GLint b)
    {
        GLubyte *data = new GLubyte[3 * width * height];

        for (int i = 0; i < width * height; i+=1)
        {
            data[3 * i + 0] = static_cast<GLubyte>(r);
            data[3 * i + 1] = static_cast<GLubyte>(g);
            data[3 * i + 2] = static_cast<GLubyte>(b);
        }

        return data;
    }

    void clearTextureLevel0(GLenum textarget,
                            GLuint texture,
                            GLfloat red,
                            GLfloat green,
                            GLfloat blue,
                            GLfloat alpha)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mOffscreenFramebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textarget, texture, 0);
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        glClearColor(red, green, blue, alpha);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    GLuint m2DProgram;
    GLuint mCubeProgram;
    GLuint mTexture2D;
    GLuint mTextureCube;

    GLubyte* mLevelZeroBlueInitData;
    GLubyte* mLevelZeroWhiteInitData;
    GLubyte* mLevelOneInitData;
    GLubyte* mLevelTwoInitData;

  private:
    GLuint mOffscreenFramebuffer;
};

class MipmapTestES3 : public BaseMipmapTest
{
  protected:
    MipmapTestES3()
        : mTexture(0),
          mArrayProgram(0),
          mTextureArraySliceUniformLocation(-1),
          m3DProgram(0),
          mTexture3DSliceUniformLocation(-1),
          mTexture3DLODUniformLocation(-1),
          m2DProgram(0)

    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    std::string vertexShaderSource()
    {
        // Don't put "#version ..." on its own line. See [cpp]p1:
        // "If there are sequences of preprocessing tokens within the list of arguments that
        //  would otherwise act as preprocessing directives, the behavior is undefined"
        // clang-format off
        return SHADER_SOURCE
        (   #version 300 es\n
            precision highp float;
            in vec4 position;
            out vec2 texcoord;

            void main()
            {
                gl_Position = vec4(position.xy, 0.0, 1.0);
                texcoord = (position.xy * 0.5) + 0.5;
            }
        );
        // clang-format on
    }

    void setUpArrayProgram()
    {
        const std::string fragmentShaderSourceArray = SHADER_SOURCE
        (   #version 300 es\n
            precision highp float;
            uniform highp sampler2DArray tex;
            uniform int slice;
            in vec2 texcoord;
            out vec4 out_FragColor;

            void main()
            {
                out_FragColor = texture(tex, vec3(texcoord, float(slice)));
            }
        );

        mArrayProgram = CompileProgram(vertexShaderSource(), fragmentShaderSourceArray);
        if (mArrayProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        mTextureArraySliceUniformLocation = glGetUniformLocation(mArrayProgram, "slice");
        ASSERT_NE(-1, mTextureArraySliceUniformLocation);

        glUseProgram(mArrayProgram);
        glUseProgram(0);
        ASSERT_GL_NO_ERROR();
    }

    void setUp3DProgram()
    {
        const std::string fragmentShaderSource3D = SHADER_SOURCE
        (   #version 300 es\n
            precision highp float;
            uniform highp sampler3D tex;
            uniform float slice;
            uniform float lod;
            in vec2 texcoord;
            out vec4 out_FragColor;

            void main()
            {
                out_FragColor = textureLod(tex, vec3(texcoord, slice), lod);
            }
        );

        m3DProgram = CompileProgram(vertexShaderSource(), fragmentShaderSource3D);
        if (m3DProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        mTexture3DSliceUniformLocation = glGetUniformLocation(m3DProgram, "slice");
        ASSERT_NE(-1, mTexture3DSliceUniformLocation);

        mTexture3DLODUniformLocation = glGetUniformLocation(m3DProgram, "lod");
        ASSERT_NE(-1, mTexture3DLODUniformLocation);

        glUseProgram(m3DProgram);
        glUniform1f(mTexture3DLODUniformLocation, 0);
        glUseProgram(0);
        ASSERT_GL_NO_ERROR();
    }

    void setUp2DProgram()
    {
        // clang-format off
        const std::string fragmentShaderSource2D = SHADER_SOURCE
        (   #version 300 es\n
            precision highp float;
            uniform highp sampler2D tex;
            in vec2 texcoord;
            out vec4 out_FragColor;

            void main()
            {
                out_FragColor = texture(tex, texcoord);
            }
        );
        // clang-format on

        m2DProgram = CompileProgram(vertexShaderSource(), fragmentShaderSource2D);
        ASSERT_NE(0u, m2DProgram);

        ASSERT_GL_NO_ERROR();
    }

    void setUpCubeProgram()
    {
        // A very simple fragment shader to sample from the negative-Y face of a texture cube.
        // clang-format off
        const std::string cubeFS = SHADER_SOURCE
        (   #version 300 es\n
            precision mediump float;
            uniform samplerCube uTexture;
            in vec2 texcoord;
            out vec4 out_FragColor;

            void main()
            {
                out_FragColor = texture(uTexture, vec3(texcoord.x, -1, texcoord.y));
            }
        );
        // clang-format on

        mCubeProgram = CompileProgram(vertexShaderSource(), cubeFS);
        ASSERT_NE(0u, mCubeProgram);

        ASSERT_GL_NO_ERROR();
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        glGenTextures(1, &mTexture);
        ASSERT_GL_NO_ERROR();

        setUpArrayProgram();
        setUp3DProgram();
        setUp2DProgram();
        setUpCubeProgram();
    }

    void TearDown() override
    {
        glDeleteTextures(1, &mTexture);

        glDeleteProgram(mArrayProgram);
        glDeleteProgram(m3DProgram);
        glDeleteProgram(m2DProgram);
        glDeleteProgram(mCubeProgram);

        ANGLETest::TearDown();
    }

    GLuint mTexture;

    GLuint mArrayProgram;
    GLint mTextureArraySliceUniformLocation;

    GLuint m3DProgram;
    GLint mTexture3DSliceUniformLocation;
    GLint mTexture3DLODUniformLocation;

    GLuint m2DProgram;

    GLuint mCubeProgram;
};

// This test uses init data for the first three levels of the texture. It passes the level 0 data in, then renders, then level 1, then renders, etc.
// This ensures that renderers using the zero LOD workaround (e.g. D3D11 FL9_3) correctly pass init data to the mipmapped texture,
// even if the the zero-LOD texture is currently in use.
TEST_P(MipmapTest, DISABLED_ThreeLevelsInitData)
{
    // Pass in level zero init data.
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, getWindowWidth(), getWindowHeight(), 0, GL_RGB, GL_UNSIGNED_BYTE, mLevelZeroBlueInitData);
    ASSERT_GL_NO_ERROR();

    // Disable mips.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // Draw a full-sized quad, and check it's blue.
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::blue);

    // Draw a half-sized quad, and check it's blue.
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 2, getWindowHeight() / 2);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::blue);

    // Draw a quarter-sized quad, and check it's blue.
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::blue);

    // Complete the texture by initializing the remaining levels.
    int n = 1;
    while (getWindowWidth() / (1U << n) >= 1)
    {
        glTexImage2D(GL_TEXTURE_2D, n, GL_RGB, getWindowWidth() / (1U << n), getWindowWidth() / (1U << n), 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        ASSERT_GL_NO_ERROR();
        n+=1;
    }

    // Pass in level one init data.
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGB, getWindowWidth() / 2, getWindowHeight() / 2, 0, GL_RGB, GL_UNSIGNED_BYTE, mLevelOneInitData);
    ASSERT_GL_NO_ERROR();

    // Draw a full-sized quad, and check it's blue.
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::blue);

    // Draw a half-sized quad, and check it's blue. We've not enabled mipmaps yet, so our init data for level one shouldn't be used.
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 2, getWindowHeight() / 2);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::blue);

    // Enable mipmaps.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);

    // Draw a half-sized quad, and check it's green.
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 2, getWindowHeight() / 2);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::green);

    // Draw a quarter-sized quad, and check it's black, since we've not passed any init data for level two.
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::black);

    // Pass in level two init data.
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGB, getWindowWidth() / 4, getWindowHeight() / 4, 0, GL_RGB, GL_UNSIGNED_BYTE, mLevelTwoInitData);
    ASSERT_GL_NO_ERROR();

    // Draw a full-sized quad, and check it's blue.
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::blue);

    // Draw a half-sized quad, and check it's green.
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 2, getWindowHeight() / 2);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::green);

    // Draw a quarter-sized quad, and check it's red.
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::red);

    // Now disable mipmaps again, and render multiple sized quads. They should all be blue, since level 0 is blue.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::blue);
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 2, getWindowHeight() / 2);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::blue);
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::blue);

    // Now reset level 0 to white, keeping mipmaps disabled. Then, render various sized quads. They should be white.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, getWindowWidth(), getWindowHeight(), 0, GL_RGB, GL_UNSIGNED_BYTE, mLevelZeroWhiteInitData);
    ASSERT_GL_NO_ERROR();

    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::white);
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 2, getWindowHeight() / 2);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::white);
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::white);

    // Then enable mipmaps again. The quads should be white, green, red respectively.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);

    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::white);
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 2, getWindowHeight() / 2);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::green);
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::red);
}

// This test generates (and uses) mipmaps on a texture using init data. D3D11 will use a non-renderable TextureStorage for this.
// The test then disables mips, renders to level zero of the texture, and reenables mips before using the texture again.
// To do this, D3D11 has to convert the TextureStorage into a renderable one.
// This test ensures that the conversion works correctly.
// In particular, on D3D11 Feature Level 9_3 it ensures that both the zero LOD workaround texture AND the 'normal' texture are copied during conversion.
TEST_P(MipmapTest, GenerateMipmapFromInitDataThenRender)
{
    // Pass in initial data so the texture is blue.
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, getWindowWidth(), getWindowHeight(), 0, GL_RGB, GL_UNSIGNED_BYTE, mLevelZeroBlueInitData);

    // Then generate the mips.
    glGenerateMipmap(GL_TEXTURE_2D);
    ASSERT_GL_NO_ERROR();

    // Enable mipmaps.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);

    // Now draw the texture to various different sized areas.
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::blue);

    // Use mip level 1
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 2, getWindowHeight() / 2);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::blue);

    // Use mip level 2
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::blue);

    ASSERT_GL_NO_ERROR();

    // Disable mips. Render a quad using the texture and ensure it's blue.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::blue);

    // Clear level 0 of the texture to red.
    clearTextureLevel0(GL_TEXTURE_2D, mTexture2D, 1.0f, 0.0f, 0.0f, 1.0f);

    // Reenable mips, and try rendering different-sized quads.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);

    // Level 0 is now red, so this should render red.
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);

    // Use mip level 1, blue.
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 2, getWindowHeight() / 2);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::blue);

    // Use mip level 2, blue.
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::blue);
}

// This test ensures that mips are correctly generated from a rendered image.
// In particular, on D3D11 Feature Level 9_3, the clear call will be performed on the zero-level texture, rather than the mipped one.
// The test ensures that the zero-level texture is correctly copied into the mipped texture before the mipmaps are generated.
TEST_P(MipmapTest, GenerateMipmapFromRenderedImage)
{
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    // Clear the texture to blue.
    clearTextureLevel0(GL_TEXTURE_2D, mTexture2D, 0.0f, 0.0f, 1.0f, 1.0f);

    // Then generate the mips
    glGenerateMipmap(GL_TEXTURE_2D);
    ASSERT_GL_NO_ERROR();

    // Enable mips.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);

    // Now draw the texture to various different sized areas.
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::blue);

    // Use mip level 1
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 2, getWindowHeight() / 2);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::blue);

    // Use mip level 2
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::blue);
}

// Test to ensure that rendering to a mipmapped texture works, regardless of whether mipmaps are enabled or not.
// TODO: This test hits a texture rebind bug in the D3D11 renderer. Fix this.
TEST_P(MipmapTest, RenderOntoLevelZeroAfterGenerateMipmap)
{
    // TODO(geofflang): Figure out why this is broken on AMD OpenGL
    if ((IsAMD() || IsIntel()) && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
    {
        std::cout << "Test skipped on Intel/AMD OpenGL." << std::endl;
        return;
    }

    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    // Clear the texture to blue.
    clearTextureLevel0(GL_TEXTURE_2D, mTexture2D, 0.0f, 0.0f, 1.0f, 1.0f);

    // Now, draw the texture to a quad that's the same size as the texture. This draws to the default framebuffer.
    // The quad should be blue.
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::blue);

    // Now go back to the texture, and generate mips on it.
    glGenerateMipmap(GL_TEXTURE_2D);
    ASSERT_GL_NO_ERROR();

    // Now try rendering the textured quad again. Note: we've not told GL to use the generated mips.
    // The quad should be blue.
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::blue);

    // Now tell GL to use the generated mips.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    EXPECT_GL_NO_ERROR();

    // Now render the textured quad again. It should be still be blue.
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::blue);

    // Now render the textured quad to an area smaller than the texture (i.e. to force minification). This should be blue.
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::blue);

    // Now clear the texture to green. This just clears the top level. The lower mips should remain blue.
    clearTextureLevel0(GL_TEXTURE_2D, mTexture2D, 0.0f, 1.0f, 0.0f, 1.0f);

    // Render a textured quad equal in size to the texture. This should be green, since we just cleared level 0.
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::green);

    // Render a small textured quad. This forces minification, so should render blue (the color of levels 1+).
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::blue);

    // Disable mipmaps again
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ASSERT_GL_NO_ERROR();

    // Render a textured quad equal in size to the texture. This should be green, the color of level 0 in the texture.
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::green);

    // Render a small textured quad. This would force minification if mips were enabled, but they're not. Therefore, this should be green.
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::green);
}

// This test ensures that the level-zero workaround for TextureCubes (on D3D11 Feature Level 9_3)
// works as expected. It tests enabling/disabling mipmaps, generating mipmaps, and rendering to level zero.
TEST_P(MipmapTest, TextureCubeGeneralLevelZero)
{
    glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureCube);

    // Draw. Since the negative-Y face's is blue, this should be blue.
    clearAndDrawQuad(mCubeProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    // Generate mipmaps, and render. This should be blue.
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    clearAndDrawQuad(mCubeProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    // Draw using a smaller viewport (to force a lower LOD of the texture). This should still be blue.
    clearAndDrawQuad(mCubeProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    // Now clear the negative-Y face of the cube to red.
    clearTextureLevel0(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, mTextureCube, 1.0f, 0.0f, 0.0f, 1.0f);

    // Draw using a full-size viewport. This should be red.
    clearAndDrawQuad(mCubeProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Draw using a quarter-size viewport, to force a lower LOD. This should be *BLUE*, since we only cleared level zero
    // of the negative-Y face to red, and left its mipmaps blue.
    clearAndDrawQuad(mCubeProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    // Disable mipmaps again, and draw a to a quarter-size viewport.
    // Since this should use level zero of the texture, this should be *RED*.
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    clearAndDrawQuad(mCubeProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// This test ensures that rendering to level-zero of a TextureCube works as expected.
TEST_P(MipmapTest, TextureCubeRenderToLevelZero)
{
    glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureCube);

    // Draw. Since the negative-Y face's is blue, this should be blue.
    clearAndDrawQuad(mCubeProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    // Now clear the negative-Y face of the cube to red.
    clearTextureLevel0(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, mTextureCube, 1.0f, 0.0f, 0.0f, 1.0f);

    // Draw using a full-size viewport. This should be red.
    clearAndDrawQuad(mCubeProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Draw a to a quarter-size viewport. This should also be red.
    clearAndDrawQuad(mCubeProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// Creates a mipmapped 2D array texture with three layers, and calls ANGLE's GenerateMipmap.
// Then tests if the mipmaps are rendered correctly for all three layers.
TEST_P(MipmapTestES3, MipmapsForTextureArray)
{
    int px = getWindowWidth() / 2;
    int py = getWindowHeight() / 2;

    glBindTexture(GL_TEXTURE_2D_ARRAY, mTexture);

    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 5, GL_RGBA8, 16, 16, 3);

    // Fill the first layer with red
    std::vector<GLColor> pixelsRed(16 * 16, GLColor::red);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 16, 16, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    pixelsRed.data());

    // Fill the second layer with green
    std::vector<GLColor> pixelsGreen(16 * 16, GLColor::green);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, 16, 16, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    pixelsGreen.data());

    // Fill the third layer with blue
    std::vector<GLColor> pixelsBlue(16 * 16, GLColor::blue);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 2, 16, 16, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    pixelsBlue.data());

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    EXPECT_GL_NO_ERROR();

    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    EXPECT_GL_NO_ERROR();

    glUseProgram(mArrayProgram);

    EXPECT_GL_NO_ERROR();

    // Draw the first slice
    glUniform1i(mTextureArraySliceUniformLocation, 0);
    drawQuad(mArrayProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::red);

    // Draw the second slice
    glUniform1i(mTextureArraySliceUniformLocation, 1);
    drawQuad(mArrayProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::green);

    // Draw the third slice
    glUniform1i(mTextureArraySliceUniformLocation, 2);
    drawQuad(mArrayProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::blue);
}

// Create a mipmapped 2D array texture with more layers than width / height, and call
// GenerateMipmap.
TEST_P(MipmapTestES3, MipmapForDeepTextureArray)
{
    int px = getWindowWidth() / 2;
    int py = getWindowHeight() / 2;

    glBindTexture(GL_TEXTURE_2D_ARRAY, mTexture);

    // Fill the whole texture with red.
    std::vector<GLColor> pixelsRed(2 * 2 * 4, GLColor::red);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 2, 2, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelsRed.data());

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    EXPECT_GL_NO_ERROR();

    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    EXPECT_GL_NO_ERROR();

    glUseProgram(mArrayProgram);

    EXPECT_GL_NO_ERROR();

    // Draw the first slice
    glUniform1i(mTextureArraySliceUniformLocation, 0);
    drawQuad(mArrayProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::red);

    // Draw the fourth slice
    glUniform1i(mTextureArraySliceUniformLocation, 3);
    drawQuad(mArrayProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::red);
}

// Creates a mipmapped 3D texture with two layers, and calls ANGLE's GenerateMipmap.
// Then tests if the mipmaps are rendered correctly for all two layers.
TEST_P(MipmapTestES3, MipmapsForTexture3D)
{
    int px = getWindowWidth() / 2;
    int py = getWindowHeight() / 2;

    glBindTexture(GL_TEXTURE_3D, mTexture);

    glTexStorage3D(GL_TEXTURE_3D, 5, GL_RGBA8, 16, 16, 2);

    // Fill the first layer with red
    std::vector<GLColor> pixelsRed(16 * 16, GLColor::red);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 16, 16, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    pixelsRed.data());

    // Fill the second layer with green
    std::vector<GLColor> pixelsGreen(16 * 16, GLColor::green);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 1, 16, 16, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    pixelsGreen.data());

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    EXPECT_GL_NO_ERROR();

    glGenerateMipmap(GL_TEXTURE_3D);

    EXPECT_GL_NO_ERROR();

    glUseProgram(m3DProgram);

    EXPECT_GL_NO_ERROR();

    // Mipmap level 0
    // Draw the first slice
    glUniform1f(mTexture3DLODUniformLocation, 0.);
    glUniform1f(mTexture3DSliceUniformLocation, 0.25f);
    drawQuad(m3DProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::red);

    // Draw the second slice
    glUniform1f(mTexture3DSliceUniformLocation, 0.75f);
    drawQuad(m3DProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::green);

    // Mipmap level 1
    // The second mipmap should only have one slice.
    glUniform1f(mTexture3DLODUniformLocation, 1.);
    drawQuad(m3DProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(px, py, 127, 127, 0, 255, 1.0);

    glUniform1f(mTexture3DSliceUniformLocation, 0.75f);
    drawQuad(m3DProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(px, py, 127, 127, 0, 255, 1.0);
}

// Create a 2D texture with levels 0-2, call GenerateMipmap with base level 1 so that level 0 stays
// the same, and then sample levels 0 and 2.
// GLES 3.0.4 section 3.8.10:
// "Mipmap generation replaces texel array levels levelbase + 1 through q with arrays derived from
// the levelbase array, regardless of their previous contents. All other mipmap arrays, including
// the levelbase array, are left unchanged by this computation."
TEST_P(MipmapTestES3, GenerateMipmapBaseLevel)
{
    if (IsAMD() && IsDesktopOpenGL())
    {
        // Observed incorrect rendering on AMD, sampling level 2 returns black.
        std::cout << "Test skipped on AMD OpenGL." << std::endl;
        return;
    }

    glBindTexture(GL_TEXTURE_2D, mTexture);

    // Fill level 0 with blue
    std::vector<GLColor> pixelsBlue(getWindowWidth() * getWindowHeight(), GLColor::blue);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixelsBlue.data());

    // Fill level 1 with red
    std::vector<GLColor> pixelsRed(getWindowWidth() * getWindowHeight() / 4, GLColor::red);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, getWindowWidth() / 2, getWindowHeight() / 2, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixelsRed.data());

    // Fill level 2 with green
    std::vector<GLColor> pixelsGreen(getWindowWidth() * getWindowHeight() / 16, GLColor::green);
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, getWindowWidth() / 4, getWindowHeight() / 4, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixelsGreen.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);

    EXPECT_GL_NO_ERROR();

    // The blue level 0 should be untouched by this since base level is 1.
    glGenerateMipmap(GL_TEXTURE_2D);

    EXPECT_GL_NO_ERROR();

    // Draw using level 2. It should be set to red by GenerateMipmap.
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::red);

    if (IsNVIDIA() && IsOpenGL())
    {
        // Observed incorrect rendering on NVIDIA, level zero seems to be incorrectly affected by
        // GenerateMipmap.
        std::cout << "Test partially skipped on NVIDIA OpenGL." << std::endl;
        return;
    }

    // Draw using level 0. It should still be blue.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::blue);
}

// Create a cube map with levels 0-2, call GenerateMipmap with base level 1 so that level 0 stays
// the same, and then sample levels 0 and 2.
// GLES 3.0.4 section 3.8.10:
// "Mipmap generation replaces texel array levels levelbase + 1 through q with arrays derived from
// the levelbase array, regardless of their previous contents. All other mipmap arrays, including
// the levelbase array, are left unchanged by this computation."
TEST_P(MipmapTestES3, GenerateMipmapCubeBaseLevel)
{
    if (IsAMD() && IsDesktopOpenGL())
    {
        // Observed incorrect rendering on AMD, sampling level 2 returns black.
        std::cout << "Test skipped on AMD OpenGL." << std::endl;
        return;
    }

    ASSERT_EQ(getWindowWidth(), getWindowHeight());

    glBindTexture(GL_TEXTURE_CUBE_MAP, mTexture);
    std::vector<GLColor> pixelsBlue(getWindowWidth() * getWindowWidth(), GLColor::blue);
    TexImageCubeMapFaces(0, GL_RGBA8, getWindowWidth(), GL_RGBA, GL_UNSIGNED_BYTE,
                         pixelsBlue.data());

    // Fill level 1 with red
    std::vector<GLColor> pixelsRed(getWindowWidth() * getWindowWidth() / 4, GLColor::red);
    TexImageCubeMapFaces(1, GL_RGBA8, getWindowWidth() / 2, GL_RGBA, GL_UNSIGNED_BYTE,
                         pixelsRed.data());

    // Fill level 2 with green
    std::vector<GLColor> pixelsGreen(getWindowWidth() * getWindowWidth() / 16, GLColor::green);
    TexImageCubeMapFaces(2, GL_RGBA8, getWindowWidth() / 4, GL_RGBA, GL_UNSIGNED_BYTE,
                         pixelsGreen.data());

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 1);

    EXPECT_GL_NO_ERROR();

    // The blue level 0 should be untouched by this since base level is 1.
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    EXPECT_GL_NO_ERROR();

    // Draw using level 2. It should be set to red by GenerateMipmap.
    clearAndDrawQuad(mCubeProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::red);

    if (IsNVIDIA() && IsOpenGL())
    {
        // Observed incorrect rendering on NVIDIA, level zero seems to be incorrectly affected by
        // GenerateMipmap.
        std::cout << "Test partially skipped on NVIDIA OpenGL." << std::endl;
        return;
    }

    // Draw using level 0. It should still be blue.
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    clearAndDrawQuad(mCubeProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::blue);
}

// Create a texture with levels 0-2, call GenerateMipmap with max level 1 so that level 2 stays the
// same, and then sample levels 1 and 2.
// GLES 3.0.4 section 3.8.10:
// "Mipmap generation replaces texel array levels levelbase + 1 through q with arrays derived from
// the levelbase array, regardless of their previous contents. All other mipmap arrays, including
// the levelbase array, are left unchanged by this computation."
TEST_P(MipmapTestES3, GenerateMipmapMaxLevel)
{
    glBindTexture(GL_TEXTURE_2D, mTexture);

    // Fill level 0 with blue
    std::vector<GLColor> pixelsBlue(getWindowWidth() * getWindowHeight(), GLColor::blue);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixelsBlue.data());

    // Fill level 1 with red
    std::vector<GLColor> pixelsRed(getWindowWidth() * getWindowHeight() / 4, GLColor::red);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, getWindowWidth() / 2, getWindowHeight() / 2, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixelsRed.data());

    // Fill level 2 with green
    std::vector<GLColor> pixelsGreen(getWindowWidth() * getWindowHeight() / 16, GLColor::green);
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, getWindowWidth() / 4, getWindowHeight() / 4, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixelsGreen.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

    EXPECT_GL_NO_ERROR();

    // The green level 2 should be untouched by this since max level is 1.
    glGenerateMipmap(GL_TEXTURE_2D);

    EXPECT_GL_NO_ERROR();

    // Draw using level 1. It should be set to blue by GenerateMipmap.
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 2, getWindowHeight() / 2);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::blue);

    // Draw using level 2. It should still be green.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2);
    clearAndDrawQuad(m2DProgram, getWindowWidth() / 4, getWindowHeight() / 4);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 8, getWindowHeight() / 8, GLColor::green);
}

// Call GenerateMipmap with out-of-range base level. The spec is interpreted so that an out-of-range
// base level does not have a color-renderable/texture-filterable internal format, so the
// GenerateMipmap call generates INVALID_OPERATION. GLES 3.0.4 section 3.8.10:
// "If the levelbase array was not specified with an unsized internal format from table 3.3 or a
// sized internal format that is both color-renderable and texture-filterable according to table
// 3.13, an INVALID_OPERATION error is generated."
TEST_P(MipmapTestES3, GenerateMipmapBaseLevelOutOfRange)
{
    glBindTexture(GL_TEXTURE_2D, mTexture);

    // Fill level 0 with blue
    std::vector<GLColor> pixelsBlue(getWindowWidth() * getWindowHeight(), GLColor::blue);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixelsBlue.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1000);

    EXPECT_GL_NO_ERROR();

    // Expecting the out-of-range base level to be treated as not color-renderable and
    // texture-filterable.
    glGenerateMipmap(GL_TEXTURE_2D);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Draw using level 0. It should still be blue.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::blue);
}

// Call GenerateMipmap with out-of-range base level on an immutable texture. The base level should
// be clamped, so the call doesn't generate an error.
TEST_P(MipmapTestES3, GenerateMipmapBaseLevelOutOfRangeImmutableTexture)
{
    glBindTexture(GL_TEXTURE_2D, mTexture);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &GLColor::green);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1000);

    EXPECT_GL_NO_ERROR();

    // This is essentially a no-op, since the texture only has one level.
    glGenerateMipmap(GL_TEXTURE_2D);

    EXPECT_GL_NO_ERROR();

    // The only level of the texture should still be green.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    clearAndDrawQuad(m2DProgram, getWindowWidth(), getWindowHeight());
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::green);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
// Note: we run these tests against 9_3 on WARP due to hardware driver issues on Win7
ANGLE_INSTANTIATE_TEST(MipmapTest,
                       ES2_D3D9(),
                       ES2_D3D11(EGL_EXPERIMENTAL_PRESENT_PATH_COPY_ANGLE),
                       ES2_D3D11(EGL_EXPERIMENTAL_PRESENT_PATH_FAST_ANGLE),
                       ES2_D3D11_FL9_3_WARP(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES());
ANGLE_INSTANTIATE_TEST(MipmapTestES3, ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES());
