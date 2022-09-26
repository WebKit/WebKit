//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "common/mathutil.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

constexpr GLuint kPixelTolerance     = 1u;
constexpr GLfloat kPixelTolerance32F = 0.01f;

// Single compressed ETC2 block of source pixels all set red
constexpr uint8_t kCompressedImageETC2[] = {0x7E, 0x80, 0x04, 0x7F, 0x00, 0x07, 0xE0, 0x00};

// Take a pixel, and reset the components not covered by the format to default
// values. In particular, the default value for the alpha component is 255
// (1.0 as unsigned normalized fixed point value).
// For legacy formats, the components may be reordered to match the color that
// would be created if a pixel of that format was initialized from the given color
GLColor SliceFormatColor(GLenum format, GLColor full)
{
    switch (format)
    {
        case GL_RED:
            return GLColor(full.R, 0, 0, 255u);
        case GL_RG:
            return GLColor(full.R, full.G, 0, 255u);
        case GL_RGB:
            return GLColor(full.R, full.G, full.B, 255u);
        case GL_RGBA:
            return full;
        case GL_LUMINANCE:
            return GLColor(full.R, full.R, full.R, 255u);
        case GL_ALPHA:
            return GLColor(0, 0, 0, full.R);
        case GL_LUMINANCE_ALPHA:
            return GLColor(full.R, full.R, full.R, full.G);
        default:
            EXPECT_TRUE(false);
            return GLColor::white;
    }
}

GLColor16UI SliceFormatColor16UI(GLenum format, GLColor16UI full)
{
    switch (format)
    {
        case GL_RED:
            return GLColor16UI(full.R, 0, 0, 0xFFFF);
        case GL_RG:
            return GLColor16UI(full.R, full.G, 0, 0xFFFF);
        case GL_RGB:
            return GLColor16UI(full.R, full.G, full.B, 0xFFFF);
        case GL_RGBA:
            return full;
        case GL_LUMINANCE:
            return GLColor16UI(full.R, full.R, full.R, 0xFFFF);
        case GL_ALPHA:
            return GLColor16UI(0, 0, 0, full.R);
        case GL_LUMINANCE_ALPHA:
            return GLColor16UI(full.R, full.R, full.R, full.G);
        default:
            EXPECT_TRUE(false);
            return GLColor16UI(0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF);
    }
}

// As above, for 32F colors
GLColor32F SliceFormatColor32F(GLenum format, GLColor32F full)
{
    switch (format)
    {
        case GL_RED:
            return GLColor32F(full.R, 0.0f, 0.0f, 1.0f);
        case GL_RG:
            return GLColor32F(full.R, full.G, 0.0f, 1.0f);
        case GL_RGB:
            return GLColor32F(full.R, full.G, full.B, 1.0f);
        case GL_RGBA:
            return full;
        case GL_LUMINANCE:
            return GLColor32F(full.R, full.R, full.R, 1.0f);
        case GL_ALPHA:
            return GLColor32F(0.0f, 0.0f, 0.0f, full.R);
        case GL_LUMINANCE_ALPHA:
            return GLColor32F(full.R, full.R, full.R, full.G);
        default:
            EXPECT_TRUE(false);
            return GLColor32F(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

class TexCoordDrawTest : public ANGLETest<>
{
  protected:
    TexCoordDrawTest() : ANGLETest(), mProgram(0), mFramebuffer(0), mFramebufferColorTexture(0)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    virtual const char *getVertexShaderSource()
    {
        return R"(precision highp float;
attribute vec4 position;
varying vec2 texcoord;

void main()
{
    gl_Position = vec4(position.xy, 0.0, 1.0);
    texcoord = (position.xy * 0.5) + 0.5;
})";
    }

    virtual const char *getFragmentShaderSource() = 0;

    virtual void setUpProgram()
    {
        const char *vertexShaderSource   = getVertexShaderSource();
        const char *fragmentShaderSource = getFragmentShaderSource();

        mProgram = CompileProgram(vertexShaderSource, fragmentShaderSource);
        ASSERT_NE(0u, mProgram);
        ASSERT_GL_NO_ERROR();
    }

    void testSetUp() override { setUpFramebuffer(); }

    void testTearDown() override
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &mFramebuffer);
        glDeleteTextures(1, &mFramebufferColorTexture);
        glDeleteProgram(mProgram);
    }

    void setUpFramebuffer()
    {
        // We use an FBO to work around an issue where the default framebuffer applies SRGB
        // conversion (particularly known to happen incorrectly on Intel GL drivers). It's not
        // clear whether this issue can even be fixed on all backends. For example GLES 3.0.4 spec
        // section 4.4 says that the format of the default framebuffer is entirely up to the window
        // system, so it might be SRGB, and GLES 3.0 doesn't have a "FRAMEBUFFER_SRGB" to turn off
        // SRGB conversion like desktop GL does.
        // TODO(oetuaho): Get rid of this if the underlying issue is fixed.
        glGenFramebuffers(1, &mFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

        glGenTextures(1, &mFramebufferColorTexture);
        glBindTexture(GL_TEXTURE_2D, mFramebufferColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               mFramebufferColorTexture, 0);
        ASSERT_GL_NO_ERROR();
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Returns the created texture ID.
    GLuint create2DTexture()
    {
        GLuint texture2D;
        glGenTextures(1, &texture2D);
        glBindTexture(GL_TEXTURE_2D, texture2D);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        EXPECT_GL_NO_ERROR();
        return texture2D;
    }

    GLuint mProgram;
    GLuint mFramebuffer;

  private:
    GLuint mFramebufferColorTexture;
};

class Texture2DTest : public TexCoordDrawTest
{
  protected:
    Texture2DTest() : TexCoordDrawTest(), mTexture2D(0), mTexture2DUniformLocation(-1) {}

    const char *getFragmentShaderSource() override
    {
        return R"(precision highp float;
uniform sampler2D tex;
varying vec2 texcoord;

void main()
{
    gl_FragColor = texture2D(tex, texcoord);
})";
    }

    virtual const char *getTextureUniformName() { return "tex"; }

    void setUpProgram() override
    {
        TexCoordDrawTest::setUpProgram();
        mTexture2DUniformLocation = glGetUniformLocation(mProgram, getTextureUniformName());
        ASSERT_NE(-1, mTexture2DUniformLocation);
    }

    void testSetUp() override
    {
        TexCoordDrawTest::testSetUp();
        mTexture2D = create2DTexture();

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteTextures(1, &mTexture2D);
        TexCoordDrawTest::testTearDown();
    }

    // Tests CopyTexSubImage with floating point textures of various formats.
    void testFloatCopySubImage(int sourceImageChannels, int destImageChannels)
    {
        setUpProgram();

        if (getClientMajorVersion() < 3)
        {
            ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_storage") ||
                               !IsGLExtensionEnabled("GL_OES_texture_float"));

            ANGLE_SKIP_TEST_IF((sourceImageChannels < 3 || destImageChannels < 3) &&
                               !IsGLExtensionEnabled("GL_EXT_texture_rg"));

            ANGLE_SKIP_TEST_IF(destImageChannels == 3 &&
                               !IsGLExtensionEnabled("GL_CHROMIUM_color_buffer_float_rgb"));

            ANGLE_SKIP_TEST_IF(destImageChannels == 4 &&
                               !IsGLExtensionEnabled("GL_CHROMIUM_color_buffer_float_rgba"));

            ANGLE_SKIP_TEST_IF(destImageChannels <= 2);
        }
        else
        {
            ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_color_buffer_float"));

            ANGLE_SKIP_TEST_IF(destImageChannels == 3 &&
                               !IsGLExtensionEnabled("GL_CHROMIUM_color_buffer_float_rgb"));
        }

        // clang-format off
        GLfloat sourceImageData[4][16] =
        {
            { // R
                1.0f,
                0.0f,
                0.0f,
                1.0f
            },
            { // RG
                1.0f, 0.0f,
                0.0f, 1.0f,
                0.0f, 0.0f,
                1.0f, 1.0f
            },
            { // RGB
                1.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 1.0f,
                1.0f, 1.0f, 0.0f
            },
            { // RGBA
                1.0f, 0.0f, 0.0f, 1.0f,
                0.0f, 1.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f, 1.0f,
                1.0f, 1.0f, 0.0f, 1.0f
            },
        };
        // clang-format on

        GLenum imageFormats[] = {
            GL_R32F,
            GL_RG32F,
            GL_RGB32F,
            GL_RGBA32F,
        };

        GLenum sourceUnsizedFormats[] = {
            GL_RED,
            GL_RG,
            GL_RGB,
            GL_RGBA,
        };

        GLTexture textures[2];

        GLfloat *imageData         = sourceImageData[sourceImageChannels - 1];
        GLenum sourceImageFormat   = imageFormats[sourceImageChannels - 1];
        GLenum sourceUnsizedFormat = sourceUnsizedFormats[sourceImageChannels - 1];
        GLenum destImageFormat     = imageFormats[destImageChannels - 1];

        glBindTexture(GL_TEXTURE_2D, textures[0]);
        if (getClientMajorVersion() >= 3)
        {
            glTexStorage2D(GL_TEXTURE_2D, 1, sourceImageFormat, 2, 2);
        }
        else
        {
            glTexStorage2DEXT(GL_TEXTURE_2D, 1, sourceImageFormat, 2, 2);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, sourceUnsizedFormat, GL_FLOAT, imageData);

        if (sourceImageChannels < 3 && !IsGLExtensionEnabled("GL_EXT_texture_rg"))
        {
            // This is not supported
            ASSERT_GL_ERROR(GL_INVALID_OPERATION);
        }
        else
        {
            ASSERT_GL_NO_ERROR();
        }

        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0], 0);

        glBindTexture(GL_TEXTURE_2D, textures[1]);
        if (getClientMajorVersion() >= 3)
        {
            glTexStorage2D(GL_TEXTURE_2D, 1, destImageFormat, 2, 2);
        }
        else
        {
            glTexStorage2DEXT(GL_TEXTURE_2D, 1, destImageFormat, 2, 2);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 2, 2);
        ASSERT_GL_NO_ERROR();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        drawQuad(mProgram, "position", 0.5f);

        int testImageChannels = std::min(sourceImageChannels, destImageChannels);

        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
        if (testImageChannels > 1)
        {
            EXPECT_PIXEL_EQ(getWindowHeight() - 1, 0, 0, 255, 0, 255);
            EXPECT_PIXEL_EQ(getWindowHeight() - 1, getWindowWidth() - 1, 255, 255, 0, 255);
            if (testImageChannels > 2)
            {
                EXPECT_PIXEL_EQ(0, getWindowWidth() - 1, 0, 0, 255, 255);
            }
        }

        glDeleteFramebuffers(1, &fbo);

        ASSERT_GL_NO_ERROR();
    }

    void testTextureSize(int testCaseIndex);
    void testTextureSizeError();

    struct UploadThenUseStageParam
    {
        GLenum useStage;
        bool closeRenderPassAfterUse;
    };

    void testUploadThenUseInDifferentStages(const std::vector<UploadThenUseStageParam> &uses);

    GLuint mTexture2D;
    GLint mTexture2DUniformLocation;
};

class Texture2DTestES3 : public Texture2DTest
{
  protected:
    Texture2DTestES3() : Texture2DTest() {}

    const char *getVertexShaderSource() override
    {
        return "#version 300 es\n"
               "out vec2 texcoord;\n"
               "in vec4 position;\n"
               "void main()\n"
               "{\n"
               "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
               "    texcoord = (position.xy * 0.5) + 0.5;\n"
               "}\n";
    }

    const char *getFragmentShaderSource() override
    {
        return "#version 300 es\n"
               "precision highp float;\n"
               "uniform highp sampler2D tex;\n"
               "in vec2 texcoord;\n"
               "out vec4 fragColor;\n"
               "void main()\n"
               "{\n"
               "    fragColor = texture(tex, texcoord);\n"
               "}\n";
    }

    void testSetUp() override
    {
        Texture2DTest::testSetUp();
        setUpProgram();
    }

    void createImmutableTexture2D(GLuint texture,
                                  size_t width,
                                  size_t height,
                                  GLenum format,
                                  GLenum internalFormat,
                                  GLenum type,
                                  GLsizei levels,
                                  GLubyte data[4])
    {
        // Support only 1 level for now
        ASSERT(levels == 1);

        glBindTexture(GL_TEXTURE_2D, texture);

        glTexStorage2D(GL_TEXTURE_2D, levels, internalFormat, width, height);
        ASSERT_GL_NO_ERROR();

        if (data != nullptr)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, data);
            ASSERT_GL_NO_ERROR();
        }

        // Disable mipmapping
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        ASSERT_GL_NO_ERROR();
    }

    void verifyResults2D(GLuint texture, GLubyte referenceColor[4])
    {
        // Draw a quad with the target texture
        glUseProgram(mProgram);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(mTexture2DUniformLocation, 0);

        drawQuad(mProgram, "position", 0.5f);

        // Expect that the rendered quad's color is the same as the reference color with a tolerance
        // of 1
        EXPECT_PIXEL_NEAR(0, 0, referenceColor[0], referenceColor[1], referenceColor[2],
                          referenceColor[3], 1);
    }
};

class Texture2DTestES3YUV : public Texture2DTestES3
{};

class Texture2DTestES3RobustInit : public Texture2DTestES3
{
  protected:
    Texture2DTestES3RobustInit() : Texture2DTestES3() { setRobustResourceInit(true); }
};

class Texture2DBaseMaxTestES3 : public ANGLETest<>
{
  protected:
    static constexpr size_t kMip0Size   = 13;
    static constexpr uint32_t kMipCount = 4;

    Texture2DBaseMaxTestES3() : ANGLETest(), mTextureLocation(0), mLodLocation(0)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    static constexpr size_t getMipDataSize(size_t mip0Size, size_t mip)
    {
        size_t mipSize = std::max<size_t>(1u, mip0Size >> mip);
        return mipSize * mipSize;
    }

    static constexpr size_t getTotalMipDataSize(size_t mip0Size)
    {
        size_t totalCount = 0;
        for (size_t mip = 0; mip < kMipCount; ++mip)
        {
            totalCount += getMipDataSize(mip0Size, mip);
        }
        return totalCount;
    }

    static constexpr size_t getMipDataOffset(size_t mip0Size, size_t mip)
    {
        // This calculates:
        //
        //     mip == 0: 0
        //     o.w.:     sum(0, mip-1) getMipDataSize(i)
        //
        // The above can be calculated simply as:
        //
        //     (mip0 >> (kMipCount-1))^2 * (0x55555555 & ((1 << (2*mip)) - 1))
        //     \__________  ___________/   \_______________  ________________/
        //                \/                               \/
        //          last mip size                 sum(0, mip-1) (4^i)
        //
        // But let's loop explicitly for clarity.
        size_t offset = 0;
        for (size_t m = 0; m < mip; ++m)
        {
            offset += getMipDataSize(mip0Size, m);
        }
        return offset;
    }

    template <typename colorType = GLColor>
    void fillMipData(colorType *data, size_t mip0Size, const colorType mipColors[kMipCount])
    {
        for (size_t mip = 0; mip < kMipCount; ++mip)
        {
            size_t offset = getMipDataOffset(mip0Size, mip);
            size_t size   = getMipDataSize(mip0Size, mip);
            std::fill(data + offset, data + offset + size, mipColors[mip]);
        }
    }

    void initTest(bool immutable)
    {
        // Set up program to sample from specific lod level.
        mProgram.makeRaster(essl3_shaders::vs::Texture2DLod(), essl3_shaders::fs::Texture2DLod());
        ASSERT(mProgram.valid());

        glUseProgram(mProgram);

        mTextureLocation = glGetUniformLocation(mProgram, essl3_shaders::Texture2DUniform());
        ASSERT_NE(-1, mTextureLocation);

        mLodLocation = glGetUniformLocation(mProgram, essl3_shaders::LodUniform());
        ASSERT_NE(-1, mLodLocation);

        // Set up texture with a handful of lods.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTexture);

        std::array<GLColor, getTotalMipDataSize(kMip0Size)> mipData;
        fillMipData(mipData.data(), kMip0Size, kMipColors);

        if (immutable)
        {
            glTexStorage2D(GL_TEXTURE_2D, kMipCount, GL_RGBA8, kMip0Size, kMip0Size);
            for (size_t mip = 0; mip < kMipCount; ++mip)
            {
                glTexSubImage2D(GL_TEXTURE_2D, mip, 0, 0, kMip0Size >> mip, kMip0Size >> mip,
                                GL_RGBA, GL_UNSIGNED_BYTE,
                                mipData.data() + getMipDataOffset(kMip0Size, mip));
            }
        }
        else
        {
            for (size_t mip = 0; mip < kMipCount; ++mip)
            {
                glTexImage2D(GL_TEXTURE_2D, mip, GL_RGBA8, kMip0Size >> mip, kMip0Size >> mip, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE,
                             mipData.data() + getMipDataOffset(kMip0Size, mip));
            }
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        EXPECT_GL_NO_ERROR();
    }

    void setLodUniform(uint32_t lod) { glUniform1f(mLodLocation, lod); }

    void testPingPongBaseLevel(bool immutable);
    void testGenerateMipmapAfterRebase(bool immutable);

    GLProgram mProgram;
    GLTexture mTexture;
    GLint mTextureLocation;
    GLint mLodLocation;

    const GLColor kMipColors[kMipCount] = {
        GLColor::red,
        GLColor::green,
        GLColor::blue,
        GLColor::magenta,
    };
};

class TextureES31PPO
{
  protected:
    TextureES31PPO() : mVertProg(0), mFragProg(0), mPipeline(0) {}

    const char *get2DTexturedVertexShaderSource()
    {
        return "#version 310 es\n"
               "precision mediump float;\n"
               "in vec2 position;\n"
               "out vec2 texCoord;\n"
               "void main()\n"
               "{\n"
               "    gl_Position = vec4(position, 0, 1);\n"
               "    texCoord = position * 0.5 + vec2(0.5);\n"
               "}";
    }

    const char *get2DTexturedFragmentShaderSource()
    {
        return "#version 310 es\n"
               "precision mediump float;\n"
               "in vec2 texCoord;\n"
               "uniform sampler2D tex1;\n"
               "uniform sampler2D tex2;\n"
               "uniform sampler2D tex3;\n"
               "uniform sampler2D tex4;\n"
               "out vec4 color;\n"
               "void main()\n"
               "{\n"
               "    color = (texture(tex1, texCoord) + texture(tex2, texCoord) \n"
               "          +  texture(tex3, texCoord) + texture(tex4, texCoord)) * 0.25;\n"
               "}";
    }

    void bindProgramPipeline(const GLchar *vertString, const GLchar *fragString)
    {
        mVertProg = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &vertString);
        ASSERT_NE(mVertProg, 0u);
        mFragProg = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &fragString);
        ASSERT_NE(mFragProg, 0u);

        // Generate a program pipeline and attach the programs to their respective stages
        glGenProgramPipelines(1, &mPipeline);
        EXPECT_GL_NO_ERROR();
        glUseProgramStages(mPipeline, GL_VERTEX_SHADER_BIT, mVertProg);
        EXPECT_GL_NO_ERROR();
        glUseProgramStages(mPipeline, GL_FRAGMENT_SHADER_BIT, mFragProg);
        EXPECT_GL_NO_ERROR();
        glBindProgramPipeline(mPipeline);
        EXPECT_GL_NO_ERROR();
    }

    void bind2DTexturedQuadProgramPipeline()
    {
        const char *vertexShaderSource   = get2DTexturedVertexShaderSource();
        const char *fragmentShaderSource = get2DTexturedFragmentShaderSource();

        m2DTexturedQuadVertProg = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &vertexShaderSource);
        ASSERT_NE(m2DTexturedQuadVertProg, 0u);
        m2DTexturedQuadFragProg =
            glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &fragmentShaderSource);
        ASSERT_NE(m2DTexturedQuadFragProg, 0u);

        // Generate a program pipeline and attach the programs to their respective stages
        glGenProgramPipelines(1, &m2DTexturedQuadPipeline);
        EXPECT_GL_NO_ERROR();
        glUseProgramStages(m2DTexturedQuadPipeline, GL_VERTEX_SHADER_BIT, m2DTexturedQuadVertProg);
        EXPECT_GL_NO_ERROR();
        glUseProgramStages(m2DTexturedQuadPipeline, GL_FRAGMENT_SHADER_BIT,
                           m2DTexturedQuadFragProg);
        EXPECT_GL_NO_ERROR();
        glBindProgramPipeline(m2DTexturedQuadPipeline);
        EXPECT_GL_NO_ERROR();
    }

    void ppoDrawQuad(std::array<Vector3, 6> &quadVertices,
                     const std::string &positionAttribName,
                     const GLfloat positionAttribZ,
                     const GLfloat positionAttribXYScale)
    {
        glUseProgram(0);

        for (Vector3 &vertex : quadVertices)
        {
            vertex.x() *= positionAttribXYScale;
            vertex.y() *= positionAttribXYScale;
            vertex.z() = positionAttribZ;
        }

        GLint positionLocation = glGetAttribLocation(mVertProg, positionAttribName.c_str());

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, quadVertices.data());
        glEnableVertexAttribArray(positionLocation);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisableVertexAttribArray(positionLocation);
        glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    GLuint mVertProg;
    GLuint mFragProg;
    GLuint mPipeline;
    GLuint m2DTexturedQuadVertProg;
    GLuint m2DTexturedQuadFragProg;
    GLuint m2DTexturedQuadPipeline;
};

class Texture2DTestES31PPO : public TextureES31PPO, public Texture2DTest
{
  protected:
    Texture2DTestES31PPO() : TextureES31PPO(), Texture2DTest() {}

    void testSetUp() override { Texture2DTest::testSetUp(); }
};

class Texture2DIntegerAlpha1TestES3 : public Texture2DTest
{
  protected:
    Texture2DIntegerAlpha1TestES3() : Texture2DTest() {}

    const char *getVertexShaderSource() override
    {
        return "#version 300 es\n"
               "out vec2 texcoord;\n"
               "in vec4 position;\n"
               "void main()\n"
               "{\n"
               "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
               "    texcoord = (position.xy * 0.5) + 0.5;\n"
               "}\n";
    }

    const char *getFragmentShaderSource() override
    {
        return "#version 300 es\n"
               "precision highp float;\n"
               "uniform highp isampler2D tex;\n"
               "in vec2 texcoord;\n"
               "out vec4 fragColor;\n"
               "void main()\n"
               "{\n"
               "    vec4 green = vec4(0, 1, 0, 1);\n"
               "    vec4 black = vec4(0, 0, 0, 0);\n"
               "    fragColor = (texture(tex, texcoord).a == 1) ? green : black;\n"
               "}\n";
    }

    void testSetUp() override
    {
        Texture2DTest::testSetUp();
        setUpProgram();
    }
};

class Texture2DUnsignedIntegerAlpha1TestES3 : public Texture2DTest
{
  protected:
    Texture2DUnsignedIntegerAlpha1TestES3() : Texture2DTest() {}

    const char *getVertexShaderSource() override
    {
        return "#version 300 es\n"
               "out vec2 texcoord;\n"
               "in vec4 position;\n"
               "void main()\n"
               "{\n"
               "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
               "    texcoord = (position.xy * 0.5) + 0.5;\n"
               "}\n";
    }

    const char *getFragmentShaderSource() override
    {
        return "#version 300 es\n"
               "precision highp float;\n"
               "uniform highp usampler2D tex;\n"
               "in vec2 texcoord;\n"
               "out vec4 fragColor;\n"
               "void main()\n"
               "{\n"
               "    vec4 green = vec4(0, 1, 0, 1);\n"
               "    vec4 black = vec4(0, 0, 0, 0);\n"
               "    fragColor = (texture(tex, texcoord).a == 1u) ? green : black;\n"
               "}\n";
    }

    void testSetUp() override
    {
        Texture2DTest::testSetUp();
        setUpProgram();
    }
};

class Texture2DTestWithDrawScale : public Texture2DTest
{
  protected:
    Texture2DTestWithDrawScale() : Texture2DTest(), mDrawScaleUniformLocation(-1) {}

    const char *getVertexShaderSource() override
    {
        return
            R"(precision highp float;
            attribute vec4 position;
            varying vec2 texcoord;

            uniform vec2 drawScale;

            void main()
            {
                gl_Position = vec4(position.xy * drawScale, 0.0, 1.0);
                texcoord = (position.xy * 0.5) + 0.5;
            })";
    }

    void testSetUp() override
    {
        Texture2DTest::testSetUp();

        setUpProgram();

        mDrawScaleUniformLocation = glGetUniformLocation(mProgram, "drawScale");
        ASSERT_NE(-1, mDrawScaleUniformLocation);

        glUseProgram(mProgram);
        glUniform2f(mDrawScaleUniformLocation, 1.0f, 1.0f);
        glUseProgram(0);
        ASSERT_GL_NO_ERROR();
    }

    GLint mDrawScaleUniformLocation;
};

class Sampler2DAsFunctionParameterTest : public Texture2DTest
{
  protected:
    Sampler2DAsFunctionParameterTest() : Texture2DTest() {}

    const char *getFragmentShaderSource() override
    {
        return
            R"(precision highp float;
            uniform sampler2D tex;
            varying vec2 texcoord;

            vec4 computeFragColor(sampler2D aTex)
            {
                return texture2D(aTex, texcoord);
            }

            void main()
            {
                gl_FragColor = computeFragColor(tex);
            })";
    }

    void testSetUp() override
    {
        Texture2DTest::testSetUp();
        setUpProgram();
    }
};

class TextureCubeTest : public TexCoordDrawTest
{
  protected:
    TextureCubeTest()
        : TexCoordDrawTest(),
          mTexture2D(0),
          mTextureCube(0),
          mTexture2DUniformLocation(-1),
          mTextureCubeUniformLocation(-1)
    {}

    const char *getFragmentShaderSource() override
    {
        return
            R"(precision highp float;
            uniform sampler2D tex2D;
            uniform samplerCube texCube;
            varying vec2 texcoord;

            void main()
            {
                gl_FragColor = texture2D(tex2D, texcoord);
                gl_FragColor += textureCube(texCube, vec3(texcoord, 0));
            })";
    }

    void testSetUp() override
    {
        TexCoordDrawTest::testSetUp();

        glGenTextures(1, &mTextureCube);
        glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureCube);
        for (GLenum face = 0; face < 6; face++)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, nullptr);
        }
        EXPECT_GL_NO_ERROR();

        mTexture2D = create2DTexture();

        setUpProgram();

        mTexture2DUniformLocation = glGetUniformLocation(mProgram, "tex2D");
        ASSERT_NE(-1, mTexture2DUniformLocation);
        mTextureCubeUniformLocation = glGetUniformLocation(mProgram, "texCube");
        ASSERT_NE(-1, mTextureCubeUniformLocation);
    }

    void testTearDown() override
    {
        glDeleteTextures(1, &mTextureCube);
        TexCoordDrawTest::testTearDown();
    }

    GLuint mTexture2D;
    GLuint mTextureCube;
    GLint mTexture2DUniformLocation;
    GLint mTextureCubeUniformLocation;
};

class TextureCubeTestES3 : public ANGLETest<>
{
  protected:
    TextureCubeTestES3() {}
};

class SamplerArrayTest : public TexCoordDrawTest
{
  protected:
    SamplerArrayTest()
        : TexCoordDrawTest(),
          mTexture2DA(0),
          mTexture2DB(0),
          mTexture0UniformLocation(-1),
          mTexture1UniformLocation(-1)
    {}

    const char *getFragmentShaderSource() override
    {
        return
            R"(precision mediump float;
            uniform highp sampler2D tex2DArray[2];
            varying vec2 texcoord;
            void main()
            {
                gl_FragColor = texture2D(tex2DArray[0], texcoord);
                gl_FragColor += texture2D(tex2DArray[1], texcoord);
            })";
    }

    void testSetUp() override
    {
        TexCoordDrawTest::testSetUp();

        setUpProgram();

        mTexture0UniformLocation = glGetUniformLocation(mProgram, "tex2DArray[0]");
        ASSERT_NE(-1, mTexture0UniformLocation);
        mTexture1UniformLocation = glGetUniformLocation(mProgram, "tex2DArray[1]");
        ASSERT_NE(-1, mTexture1UniformLocation);

        mTexture2DA = create2DTexture();
        mTexture2DB = create2DTexture();
        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteTextures(1, &mTexture2DA);
        glDeleteTextures(1, &mTexture2DB);
        TexCoordDrawTest::testTearDown();
    }

    void testSamplerArrayDraw()
    {
        GLubyte texData[4];
        texData[0] = 0;
        texData[1] = 60;
        texData[2] = 0;
        texData[3] = 255;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTexture2DA);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, texData);

        texData[1] = 120;
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mTexture2DB);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, texData);
        EXPECT_GL_ERROR(GL_NO_ERROR);

        glUseProgram(mProgram);
        glUniform1i(mTexture0UniformLocation, 0);
        glUniform1i(mTexture1UniformLocation, 1);
        drawQuad(mProgram, "position", 0.5f);
        EXPECT_GL_NO_ERROR();

        EXPECT_PIXEL_NEAR(0, 0, 0, 180, 0, 255, 2);
    }

    GLuint mTexture2DA;
    GLuint mTexture2DB;
    GLint mTexture0UniformLocation;
    GLint mTexture1UniformLocation;
};

class SamplerArrayAsFunctionParameterTest : public SamplerArrayTest
{
  protected:
    SamplerArrayAsFunctionParameterTest() : SamplerArrayTest() {}

    const char *getFragmentShaderSource() override
    {
        return
            R"(precision mediump float;
            uniform highp sampler2D tex2DArray[2];
            varying vec2 texcoord;

            vec4 computeFragColor(highp sampler2D aTex2DArray[2])
            {
                return texture2D(aTex2DArray[0], texcoord) + texture2D(aTex2DArray[1], texcoord);
            }

            void main()
            {
                gl_FragColor = computeFragColor(tex2DArray);
            })";
    }
};

class Texture2DArrayTestES3 : public TexCoordDrawTest
{
  protected:
    Texture2DArrayTestES3()
        : TexCoordDrawTest(),
          m2DArrayTexture(0),
          mTextureArrayLocation(-1),
          mTextureArraySliceUniformLocation(-1)
    {}

    const char *getVertexShaderSource() override
    {
        return R"(#version 300 es
out vec2 texcoord;
in vec4 position;
void main()
{
    gl_Position = vec4(position.xy, 0.0, 1.0);
    texcoord = (position.xy * 0.5) + 0.5;
})";
    }

    const char *getFragmentShaderSource() override
    {
        return R"(#version 300 es
precision highp float;
uniform highp sampler2DArray tex2DArray;
uniform int slice;
in vec2 texcoord;
out vec4 fragColor;
void main()
{
    fragColor = texture(tex2DArray, vec3(texcoord, float(slice)));
})";
    }

    void testSetUp() override
    {
        TexCoordDrawTest::testSetUp();

        setUpProgram();

        mTextureArrayLocation = glGetUniformLocation(mProgram, "tex2DArray");
        ASSERT_NE(-1, mTextureArrayLocation);

        mTextureArraySliceUniformLocation = glGetUniformLocation(mProgram, "slice");
        ASSERT_NE(-1, mTextureArraySliceUniformLocation);

        glGenTextures(1, &m2DArrayTexture);
        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteTextures(1, &m2DArrayTexture);
        TexCoordDrawTest::testTearDown();
    }

    GLuint m2DArrayTexture;
    GLint mTextureArrayLocation;
    GLint mTextureArraySliceUniformLocation;
};

class TextureSizeTextureArrayTest : public TexCoordDrawTest
{
  protected:
    TextureSizeTextureArrayTest()
        : TexCoordDrawTest(),
          mTexture2DA(0),
          mTexture2DB(0),
          mTexture0Location(-1),
          mTexture1Location(-1)
    {}

    const char *getVertexShaderSource() override { return essl3_shaders::vs::Simple(); }

    const char *getFragmentShaderSource() override
    {
        return "#version 300 es\n"
               "precision highp float;\n"
               "uniform highp sampler2D tex2DArray[2];\n"
               "out vec4 fragColor;\n"
               "void main()\n"
               "{\n"
               "    float red = float(textureSize(tex2DArray[0], 0).x) / 255.0;\n"
               "    float green = float(textureSize(tex2DArray[1], 0).x) / 255.0;\n"
               "    fragColor = vec4(red, green, 0.0, 1.0);\n"
               "}\n";
    }

    void testSetUp() override
    {
        TexCoordDrawTest::testSetUp();

        setUpProgram();

        mTexture0Location = glGetUniformLocation(mProgram, "tex2DArray[0]");
        ASSERT_NE(-1, mTexture0Location);
        mTexture1Location = glGetUniformLocation(mProgram, "tex2DArray[1]");
        ASSERT_NE(-1, mTexture1Location);

        mTexture2DA = create2DTexture();
        mTexture2DB = create2DTexture();
        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteTextures(1, &mTexture2DA);
        glDeleteTextures(1, &mTexture2DB);
        TexCoordDrawTest::testTearDown();
    }

    GLuint mTexture2DA;
    GLuint mTexture2DB;
    GLint mTexture0Location;
    GLint mTexture1Location;
};

// Test for GL_OES_texture_3D extension
class Texture3DTestES2 : public TexCoordDrawTest
{
  protected:
    Texture3DTestES2() : TexCoordDrawTest(), mTexture3D(0), mTexture3DUniformLocation(-1) {}

    const char *getVertexShaderSource() override
    {
        return "#version 100\n"
               "varying vec2 texcoord;\n"
               "attribute vec4 position;\n"
               "void main()\n"
               "{\n"
               "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
               "    texcoord = (position.xy * 0.5) + 0.5;\n"
               "}\n";
    }

    const char *getFragmentShaderSource() override
    {
        if (!hasTexture3DExt())
        {
            return "#version 100\n"
                   "precision highp float;\n"
                   "varying vec2 texcoord;\n"
                   "void main()\n"
                   "{\n"
                   "    gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
                   "}\n";
        }
        return "#version 100\n"
               "#extension GL_OES_texture_3D : enable\n"
               "precision highp float;\n"
               "uniform highp sampler3D tex3D;\n"
               "uniform highp float level;\n"
               "varying vec2 texcoord;\n"
               "void main()\n"
               "{\n"
               "    gl_FragColor = texture3DLod(tex3D, vec3(texcoord, 0.0), level);\n"
               "}\n";
    }

    void testSetUp() override
    {
        // http://anglebug.com/5728
        ANGLE_SKIP_TEST_IF(IsOzone());

        TexCoordDrawTest::testSetUp();

        glGenTextures(1, &mTexture3D);

        setUpProgram();

        mTexture3DUniformLocation = glGetUniformLocation(mProgram, "tex3D");
        if (hasTexture3DExt())
        {
            ASSERT_NE(-1, mTexture3DUniformLocation);
        }
    }

    void testTearDown() override
    {
        glDeleteTextures(1, &mTexture3D);
        TexCoordDrawTest::testTearDown();
    }

    bool hasTexture3DExt() const
    {
        // http://anglebug.com/4927
        if ((IsPixel2() || IsNexus5X()) && IsOpenGLES())
        {
            return false;
        }
        return IsGLExtensionEnabled("GL_OES_texture_3D");
    }

    GLuint mTexture3D;
    GLint mTexture3DUniformLocation;
};

class Texture3DTestES3 : public Texture3DTestES2
{
  protected:
    Texture3DTestES3() : Texture3DTestES2() {}

    const char *getVertexShaderSource() override
    {
        return "#version 300 es\n"
               "out vec2 texcoord;\n"
               "in vec4 position;\n"
               "void main()\n"
               "{\n"
               "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
               "    texcoord = (position.xy * 0.5) + 0.5;\n"
               "}\n";
    }

    const char *getFragmentShaderSource() override
    {
        return "#version 300 es\n"
               "precision highp float;\n"
               "uniform highp sampler3D tex3D;\n"
               "in vec2 texcoord;\n"
               "out vec4 fragColor;\n"
               "void main()\n"
               "{\n"
               "    fragColor = texture(tex3D, vec3(texcoord, 0.0));\n"
               "}\n";
    }
};

class ShadowSamplerPlusSampler3DTestES3 : public TexCoordDrawTest
{
  protected:
    ShadowSamplerPlusSampler3DTestES3()
        : TexCoordDrawTest(),
          mTextureShadow(0),
          mTexture3D(0),
          mTextureShadowUniformLocation(-1),
          mTexture3DUniformLocation(-1),
          mDepthRefUniformLocation(-1)
    {}

    const char *getVertexShaderSource() override
    {
        return "#version 300 es\n"
               "out vec2 texcoord;\n"
               "in vec4 position;\n"
               "void main()\n"
               "{\n"
               "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
               "    texcoord = (position.xy * 0.5) + 0.5;\n"
               "}\n";
    }

    const char *getFragmentShaderSource() override
    {
        return "#version 300 es\n"
               "precision highp float;\n"
               "uniform highp sampler2DShadow tex2DShadow;\n"
               "uniform highp sampler3D tex3D;\n"
               "in vec2 texcoord;\n"
               "uniform float depthRef;\n"
               "out vec4 fragColor;\n"
               "void main()\n"
               "{\n"
               "    fragColor = vec4(texture(tex2DShadow, vec3(texcoord, depthRef)) * 0.5);\n"
               "    fragColor += texture(tex3D, vec3(texcoord, 0.0));\n"
               "}\n";
    }

    void testSetUp() override
    {
        TexCoordDrawTest::testSetUp();

        glGenTextures(1, &mTexture3D);

        glGenTextures(1, &mTextureShadow);
        glBindTexture(GL_TEXTURE_2D, mTextureShadow);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

        setUpProgram();

        mTextureShadowUniformLocation = glGetUniformLocation(mProgram, "tex2DShadow");
        ASSERT_NE(-1, mTextureShadowUniformLocation);
        mTexture3DUniformLocation = glGetUniformLocation(mProgram, "tex3D");
        ASSERT_NE(-1, mTexture3DUniformLocation);
        mDepthRefUniformLocation = glGetUniformLocation(mProgram, "depthRef");
        ASSERT_NE(-1, mDepthRefUniformLocation);
    }

    void testTearDown() override
    {
        glDeleteTextures(1, &mTextureShadow);
        glDeleteTextures(1, &mTexture3D);
        TexCoordDrawTest::testTearDown();
    }

    GLuint mTextureShadow;
    GLuint mTexture3D;
    GLint mTextureShadowUniformLocation;
    GLint mTexture3DUniformLocation;
    GLint mDepthRefUniformLocation;
};

class SamplerTypeMixTestES3 : public TexCoordDrawTest
{
  protected:
    SamplerTypeMixTestES3()
        : TexCoordDrawTest(),
          mTexture2D(0),
          mTextureCube(0),
          mTexture2DShadow(0),
          mTextureCubeShadow(0),
          mTexture2DUniformLocation(-1),
          mTextureCubeUniformLocation(-1),
          mTexture2DShadowUniformLocation(-1),
          mTextureCubeShadowUniformLocation(-1),
          mDepthRefUniformLocation(-1)
    {}

    const char *getVertexShaderSource() override
    {
        return "#version 300 es\n"
               "out vec2 texcoord;\n"
               "in vec4 position;\n"
               "void main()\n"
               "{\n"
               "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
               "    texcoord = (position.xy * 0.5) + 0.5;\n"
               "}\n";
    }

    const char *getFragmentShaderSource() override
    {
        return "#version 300 es\n"
               "precision highp float;\n"
               "uniform highp sampler2D tex2D;\n"
               "uniform highp samplerCube texCube;\n"
               "uniform highp sampler2DShadow tex2DShadow;\n"
               "uniform highp samplerCubeShadow texCubeShadow;\n"
               "in vec2 texcoord;\n"
               "uniform float depthRef;\n"
               "out vec4 fragColor;\n"
               "void main()\n"
               "{\n"
               "    fragColor = texture(tex2D, texcoord);\n"
               "    fragColor += texture(texCube, vec3(1.0, 0.0, 0.0));\n"
               "    fragColor += vec4(texture(tex2DShadow, vec3(texcoord, depthRef)) * 0.25);\n"
               "    fragColor += vec4(texture(texCubeShadow, vec4(1.0, 0.0, 0.0, depthRef)) * "
               "0.125);\n"
               "}\n";
    }

    void testSetUp() override
    {
        TexCoordDrawTest::testSetUp();

        glGenTextures(1, &mTexture2D);
        glGenTextures(1, &mTextureCube);

        glGenTextures(1, &mTexture2DShadow);
        glBindTexture(GL_TEXTURE_2D, mTexture2DShadow);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

        glGenTextures(1, &mTextureCubeShadow);
        glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureCubeShadow);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

        setUpProgram();

        mTexture2DUniformLocation = glGetUniformLocation(mProgram, "tex2D");
        ASSERT_NE(-1, mTexture2DUniformLocation);
        mTextureCubeUniformLocation = glGetUniformLocation(mProgram, "texCube");
        ASSERT_NE(-1, mTextureCubeUniformLocation);
        mTexture2DShadowUniformLocation = glGetUniformLocation(mProgram, "tex2DShadow");
        ASSERT_NE(-1, mTexture2DShadowUniformLocation);
        mTextureCubeShadowUniformLocation = glGetUniformLocation(mProgram, "texCubeShadow");
        ASSERT_NE(-1, mTextureCubeShadowUniformLocation);
        mDepthRefUniformLocation = glGetUniformLocation(mProgram, "depthRef");
        ASSERT_NE(-1, mDepthRefUniformLocation);

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteTextures(1, &mTexture2D);
        glDeleteTextures(1, &mTextureCube);
        glDeleteTextures(1, &mTexture2DShadow);
        glDeleteTextures(1, &mTextureCubeShadow);
        TexCoordDrawTest::testTearDown();
    }

    GLuint mTexture2D;
    GLuint mTextureCube;
    GLuint mTexture2DShadow;
    GLuint mTextureCubeShadow;
    GLint mTexture2DUniformLocation;
    GLint mTextureCubeUniformLocation;
    GLint mTexture2DShadowUniformLocation;
    GLint mTextureCubeShadowUniformLocation;
    GLint mDepthRefUniformLocation;
};

class SamplerInStructTest : public Texture2DTest
{
  protected:
    SamplerInStructTest() : Texture2DTest() {}

    const char *getTextureUniformName() override { return "us.tex"; }

    const char *getFragmentShaderSource() override
    {
        return "precision highp float;\n"
               "struct S\n"
               "{\n"
               "    vec4 a;\n"
               "    highp sampler2D tex;\n"
               "};\n"
               "uniform S us;\n"
               "varying vec2 texcoord;\n"
               "void main()\n"
               "{\n"
               "    gl_FragColor = texture2D(us.tex, texcoord + us.a.x);\n"
               "}\n";
    }

    void runSamplerInStructTest()
    {
        setUpProgram();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTexture2D);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     &GLColor::green);
        drawQuad(mProgram, "position", 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    }
};

class SamplerInStructAsFunctionParameterTest : public SamplerInStructTest
{
  protected:
    SamplerInStructAsFunctionParameterTest() : SamplerInStructTest() {}

    const char *getFragmentShaderSource() override
    {
        return "precision highp float;\n"
               "struct S\n"
               "{\n"
               "    vec4 a;\n"
               "    highp sampler2D tex;\n"
               "};\n"
               "uniform S us;\n"
               "varying vec2 texcoord;\n"
               "vec4 sampleFrom(S s) {\n"
               "    return texture2D(s.tex, texcoord + s.a.x);\n"
               "}\n"
               "void main()\n"
               "{\n"
               "    gl_FragColor = sampleFrom(us);\n"
               "}\n";
    }
};

class SamplerInStructArrayAsFunctionParameterTest : public SamplerInStructTest
{
  protected:
    SamplerInStructArrayAsFunctionParameterTest() : SamplerInStructTest() {}

    const char *getTextureUniformName() override { return "us[0].tex"; }

    const char *getFragmentShaderSource() override
    {
        return "precision highp float;\n"
               "struct S\n"
               "{\n"
               "    vec4 a;\n"
               "    highp sampler2D tex;\n"
               "};\n"
               "uniform S us[1];\n"
               "varying vec2 texcoord;\n"
               "vec4 sampleFrom(S s) {\n"
               "    return texture2D(s.tex, texcoord + s.a.x);\n"
               "}\n"
               "void main()\n"
               "{\n"
               "    gl_FragColor = sampleFrom(us[0]);\n"
               "}\n";
    }
};

class SamplerInNestedStructAsFunctionParameterTest : public SamplerInStructTest
{
  protected:
    SamplerInNestedStructAsFunctionParameterTest() : SamplerInStructTest() {}

    const char *getTextureUniformName() override { return "us[0].sub.tex"; }

    const char *getFragmentShaderSource() override
    {
        return "precision highp float;\n"
               "struct SUB\n"
               "{\n"
               "    vec4 a;\n"
               "    highp sampler2D tex;\n"
               "};\n"
               "struct S\n"
               "{\n"
               "    SUB sub;\n"
               "};\n"
               "uniform S us[1];\n"
               "varying vec2 texcoord;\n"
               "vec4 sampleFrom(SUB s) {\n"
               "    return texture2D(s.tex, texcoord + s.a.x);\n"
               "}\n"
               "void main()\n"
               "{\n"
               "    gl_FragColor = sampleFrom(us[0].sub);\n"
               "}\n";
    }
};

class SamplerInStructAndOtherVariableTest : public SamplerInStructTest
{
  protected:
    SamplerInStructAndOtherVariableTest() : SamplerInStructTest() {}

    const char *getFragmentShaderSource() override
    {
        return "precision highp float;\n"
               "struct S\n"
               "{\n"
               "    vec4 a;\n"
               "    highp sampler2D tex;\n"
               "};\n"
               "uniform S us;\n"
               "uniform float us_tex;\n"
               "varying vec2 texcoord;\n"
               "void main()\n"
               "{\n"
               "    gl_FragColor = texture2D(us.tex, texcoord + us.a.x + us_tex);\n"
               "}\n";
    }
};

class Texture2DIntegerTestES3 : public Texture2DTest
{
  protected:
    Texture2DIntegerTestES3() : Texture2DTest() {}

    const char *getVertexShaderSource() override
    {
        return "#version 300 es\n"
               "out vec2 texcoord;\n"
               "in vec4 position;\n"
               "void main()\n"
               "{\n"
               "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
               "    texcoord = (position.xy * 0.5) + 0.5;\n"
               "}\n";
    }

    const char *getFragmentShaderSource() override
    {
        return "#version 300 es\n"
               "precision highp float;\n"
               "precision highp usampler2D;\n"
               "uniform usampler2D tex;\n"
               "in vec2 texcoord;\n"
               "out vec4 fragColor;\n"
               "void main()\n"
               "{\n"
               "    fragColor = vec4(texture(tex, texcoord))/255.0;\n"
               "}\n";
    }
};

class TextureCubeIntegerTestES3 : public TexCoordDrawTest
{
  protected:
    TextureCubeIntegerTestES3()
        : TexCoordDrawTest(), mTextureCube(0), mTextureCubeUniformLocation(-1)
    {}

    const char *getVertexShaderSource() override
    {
        return "#version 300 es\n"
               "out vec2 texcoord;\n"
               "in vec4 position;\n"
               "void main()\n"
               "{\n"
               "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
               "    texcoord = 0.5*position.xy;\n"
               "}\n";
    }

    const char *getFragmentShaderSource() override
    {
        return "#version 300 es\n"
               "precision highp float;\n"
               "precision highp usamplerCube;\n"
               "uniform usamplerCube texCube;\n"
               "in vec2 texcoord;\n"
               "out vec4 fragColor;\n"
               "void main()\n"
               "{\n"
               "    fragColor = vec4(texture(texCube, vec3(texcoord, 1)))/255.0;\n"
               "}\n";
    }

    void testSetUp() override
    {
        TexCoordDrawTest::testSetUp();
        glGenTextures(1, &mTextureCube);
        setUpProgram();

        mTextureCubeUniformLocation = glGetUniformLocation(mProgram, "texCube");
        ASSERT_NE(-1, mTextureCubeUniformLocation);
    }

    void testTearDown() override
    {
        glDeleteTextures(1, &mTextureCube);
        TexCoordDrawTest::testTearDown();
    }

    GLuint mTextureCube;
    GLint mTextureCubeUniformLocation;
};

class TextureCubeIntegerEdgeTestES3 : public TextureCubeIntegerTestES3
{
  protected:
    TextureCubeIntegerEdgeTestES3() : TextureCubeIntegerTestES3() {}

    const char *getVertexShaderSource() override
    {
        return "#version 300 es\n"
               "out vec2 texcoord;\n"
               "in vec4 position;\n"
               "void main()\n"
               "{\n"
               "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
               "    texcoord = position.xy;\n"
               "}\n";
    }

    const char *getFragmentShaderSource() override
    {
        return "#version 300 es\n"
               "precision highp float;\n"
               "precision highp usamplerCube;\n"
               "uniform usamplerCube texCube;\n"
               "in vec2 texcoord;\n"
               "out vec4 fragColor;\n"
               "void main()\n"
               "{\n"
               "    fragColor = vec4(texture(texCube, vec3(texcoord, 0)))/255.0;\n"
               "}\n";
    }
};

class Texture2DIntegerProjectiveOffsetTestES3 : public Texture2DTest
{
  protected:
    Texture2DIntegerProjectiveOffsetTestES3() : Texture2DTest() {}

    const char *getVertexShaderSource() override
    {
        return "#version 300 es\n"
               "out vec2 texcoord;\n"
               "in vec4 position;\n"
               "void main()\n"
               "{\n"
               "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
               "    texcoord = 0.5*position.xy + vec2(0.5, 0.5);\n"
               "}\n";
    }

    const char *getFragmentShaderSource() override
    {
        return "#version 300 es\n"
               "precision highp float;\n"
               "precision highp usampler2D;\n"
               "uniform usampler2D tex;\n"
               "in vec2 texcoord;\n"
               "out vec4 fragColor;\n"
               "void main()\n"
               "{\n"
               "    fragColor = vec4(textureProjOffset(tex, vec3(texcoord, 1), ivec2(0,0), "
               "0.0))/255.0;\n"
               "}\n";
    }
};

class Texture2DArrayIntegerTestES3 : public Texture2DArrayTestES3
{
  protected:
    Texture2DArrayIntegerTestES3() : Texture2DArrayTestES3() {}

    const char *getVertexShaderSource() override
    {
        return R"(#version 300 es
out vec2 texcoord;
in vec4 position;
void main()
{
    gl_Position = vec4(position.xy, 0.0, 1.0);
    texcoord = (position.xy * 0.5) + 0.5;
})";
    }

    const char *getFragmentShaderSource() override
    {
        return R"(#version 300 es
precision highp float;
uniform highp usampler2DArray tex2DArray;
uniform int slice;
in vec2 texcoord;
out vec4 fragColor;
void main()
{
    fragColor = vec4(texture(tex2DArray, vec3(texcoord, slice)))/255.0;
})";
    }
};

class Texture3DIntegerTestES3 : public Texture3DTestES3
{
  protected:
    Texture3DIntegerTestES3() : Texture3DTestES3() {}

    const char *getVertexShaderSource() override
    {
        return "#version 300 es\n"
               "out vec2 texcoord;\n"
               "in vec4 position;\n"
               "void main()\n"
               "{\n"
               "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
               "    texcoord = (position.xy * 0.5) + 0.5;\n"
               "}\n";
    }

    const char *getFragmentShaderSource() override
    {
        return "#version 300 es\n"
               "precision highp float;\n"
               "uniform highp usampler3D tex3D;\n"
               "in vec2 texcoord;\n"
               "out vec4 fragColor;\n"
               "void main()\n"
               "{\n"
               "    fragColor = vec4(texture(tex3D, vec3(texcoord, 0.0)))/255.0;\n"
               "}\n";
    }
};

class PBOCompressedTextureTest : public Texture2DTest
{
  protected:
    PBOCompressedTextureTest() : Texture2DTest() {}

    void testSetUp() override
    {
        TexCoordDrawTest::testSetUp();
        glGenTextures(1, &mTexture2D);
        glBindTexture(GL_TEXTURE_2D, mTexture2D);
        EXPECT_GL_NO_ERROR();

        setUpProgram();

        glGenBuffers(1, &mPBO);
    }

    void testTearDown() override
    {
        glDeleteBuffers(1, &mPBO);
        Texture2DTest::testTearDown();
    }

    void runCompressedSubImage();

    GLuint mPBO;
};

class ETC1CompressedTextureTest : public Texture2DTest
{
  protected:
    ETC1CompressedTextureTest() : Texture2DTest() {}

    void testSetUp() override
    {
        TexCoordDrawTest::testSetUp();
        glGenTextures(1, &mTexture2D);
        glBindTexture(GL_TEXTURE_2D, mTexture2D);
        EXPECT_GL_NO_ERROR();

        setUpProgram();
    }

    void testTearDown() override { Texture2DTest::testTearDown(); }
};

class Texture2DTestES31 : public Texture2DTest
{
  protected:
    Texture2DTestES31() : Texture2DTest() {}

    void TestSampleStencilFromDepthStencil(GLenum format, bool swizzle);
};

void Texture2DTestES31::TestSampleStencilFromDepthStencil(GLenum format, bool swizzle)
{
    constexpr GLsizei kSize = 4;

    // Set up a color texture.
    GLTexture colorTexture;
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kSize, kSize);
    ASSERT_GL_NO_ERROR();

    // Set up a depth/stencil texture to be sampled as stencil.
    GLTexture depthStencilTexture;
    glBindTexture(GL_TEXTURE_2D, depthStencilTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, format, kSize, kSize);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);
    if (swizzle)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ALPHA);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_BLUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_GREEN);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
    }
    ASSERT_GL_NO_ERROR();

    constexpr char kFS[] =
        R"(#version 300 es
precision mediump float;
uniform highp usampler2D stencilTex;
out vec4 color;
void main()
{
    color = vec4(texelFetch(stencilTex, ivec2(0, 0), 0)) / 255.0f;
})";

    // Clear stencil to 42.
    GLFramebuffer clearFBO;
    glBindFramebuffer(GL_FRAMEBUFFER, clearFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                           depthStencilTexture, 0);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    glClearDepthf(0.42f);
    glClearStencil(42);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthStencilTexture);
    EXPECT_GL_ERROR(GL_NO_ERROR);

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    GLint stencilTexLocation = glGetUniformLocation(program, "stencilTex");
    ASSERT_NE(-1, stencilTexLocation);
    ASSERT_GL_NO_ERROR();

    glUseProgram(program);
    glUniform1i(stencilTexLocation, 0);

    GLFramebuffer drawFBO;
    glBindFramebuffer(GL_FRAMEBUFFER, drawFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    ASSERT_GL_NO_ERROR();

    drawQuad(program, essl3_shaders::PositionAttrib(), 0.95f);
    ASSERT_GL_NO_ERROR();

    GLColor expected = swizzle ? GLColor(1, 0, 0, 42) : GLColor(42, 0, 0, 1);
    EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, expected);
}

TEST_P(Texture2DTest, NegativeAPISubImage)
{
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    EXPECT_GL_ERROR(GL_NO_ERROR);

    setUpProgram();

    const GLubyte *pixels[20] = {0};
    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    if (IsGLExtensionEnabled("GL_EXT_texture_storage"))
    {
        // Create a 1-level immutable texture.
        glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2);

        // Try calling sub image on the second level.
        glTexSubImage2D(GL_TEXTURE_2D, 1, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
}

// Test that querying GL_TEXTURE_BINDING* doesn't cause an unexpected error.
TEST_P(Texture2DTest, QueryBinding)
{
    glBindTexture(GL_TEXTURE_2D, 0);
    EXPECT_GL_ERROR(GL_NO_ERROR);

    GLint textureBinding;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(0, textureBinding);

    glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &textureBinding);
    if (IsGLExtensionEnabled("GL_OES_EGL_image_external") ||
        IsGLExtensionEnabled("GL_NV_EGL_stream_consumer_external"))
    {
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(0, textureBinding);
    }
    else
    {
        EXPECT_GL_ERROR(GL_INVALID_ENUM);
    }
}

TEST_P(Texture2DTest, ZeroSizedUploads)
{
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    EXPECT_GL_ERROR(GL_NO_ERROR);

    setUpProgram();

    // Use the texture first to make sure it's in video memory
    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);

    const GLubyte *pixel[4] = {0};

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    EXPECT_GL_NO_ERROR();

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    EXPECT_GL_NO_ERROR();

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    EXPECT_GL_NO_ERROR();
}

// Test that interleaved superseded updates work as expected
TEST_P(Texture2DTest, InterleavedSupersedingTextureUpdates)
{
    constexpr uint32_t kTexWidth  = 3840;
    constexpr uint32_t kTexHeight = 2160;
    constexpr uint32_t kBpp       = 4;

    // Create the texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kTexWidth, kTexHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    EXPECT_GL_ERROR(GL_NO_ERROR);

    // 1. One big upload followed by many small identical uploads
    // Update the entire texture
    std::vector<GLubyte> fullTextureData(kTexWidth * kTexHeight * kBpp, 128);
    constexpr GLColor kFullTextureColor = GLColor(128u, 128u, 128u, 128u);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTexWidth, kTexHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                    fullTextureData.data());

    // Make a number of identical updates to the right half of the texture
    std::vector<GLubyte> rightHalfData(kTexWidth * kTexHeight * kBpp, 201);
    constexpr GLColor kRightHalfColor = GLColor(201u, 201u, 201u, 201u);
    for (uint32_t iteration = 0; iteration < 10; iteration++)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, kTexWidth / 2, 0, kTexWidth / 2, kTexHeight, GL_RGBA,
                        GL_UNSIGNED_BYTE, rightHalfData.data());
    }

    setUpProgram();
    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(1 * getWindowWidth() / 4, getWindowHeight() / 2, kFullTextureColor);
    EXPECT_PIXEL_COLOR_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 2, kRightHalfColor);

    // 2. Some small uploads followed by one big upload followed by many identical uploads
    // Clear the entire texture
    std::vector<GLubyte> zeroTextureData(kTexWidth * kTexHeight * kBpp, 255);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTexWidth, kTexHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                    zeroTextureData.data());

    // Update the top left quadrant of the texture
    std::vector<GLubyte> topLeftQuadrantData(kTexWidth * kTexHeight * kBpp, 128);
    constexpr GLColor kTopLeftQuandrantTextureColor = GLColor(128u, 128u, 128u, 128u);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, kTexHeight / 2, kTexWidth / 2, kTexHeight / 2, GL_RGBA,
                    GL_UNSIGNED_BYTE, topLeftQuadrantData.data());

    // Update the top right quadrant of the texture
    std::vector<GLubyte> topRightQuadrantData(kTexWidth * kTexHeight * kBpp, 156);
    constexpr GLColor kTopRightQuadrantTextureColor = GLColor(156u, 156u, 156u, 156u);
    glTexSubImage2D(GL_TEXTURE_2D, 0, kTexWidth / 2, kTexHeight / 2, kTexWidth / 2, kTexHeight / 2,
                    GL_RGBA, GL_UNSIGNED_BYTE, topRightQuadrantData.data());

    // Update the bottom half of the texture
    std::vector<GLubyte> bottomHalfTextureData(kTexWidth * kTexHeight * kBpp, 187);
    constexpr GLColor kBottomHalfTextureColor = GLColor(187u, 187u, 187u, 187u);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTexWidth, kTexHeight / 2, GL_RGBA, GL_UNSIGNED_BYTE,
                    bottomHalfTextureData.data());

    // Make a number of identical updates to the bottom right quadrant of the texture
    std::vector<GLubyte> bottomRightQuadrantData(kTexWidth * kTexHeight * kBpp, 201);
    constexpr GLColor kBottomRightQuadrantColor = GLColor(201u, 201u, 201u, 201u);
    for (uint32_t iteration = 0; iteration < 10; iteration++)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, kTexWidth / 2, 0, kTexWidth / 2, kTexHeight / 2, GL_RGBA,
                        GL_UNSIGNED_BYTE, bottomRightQuadrantData.data());
    }

    setUpProgram();
    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, getWindowHeight() - 1, kTopLeftQuandrantTextureColor);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1,
                          kTopRightQuadrantTextureColor);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, kBottomHalfTextureColor);
    EXPECT_PIXEL_COLOR_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4,
                          kBottomRightQuadrantColor);

    // 3. Many small uploads folloed by one big upload
    // Clear the entire texture
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTexWidth, kTexHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                    zeroTextureData.data());

    // Make a number of small updates to different parts of the texture
    std::vector<std::pair<GLint, GLint>> xyOffsets = {
        {1, 4}, {128, 34}, {1208, 1090}, {2560, 2022}};
    constexpr GLColor kRandomColor = GLColor(55u, 128u, 201u, 255u);
    for (const std::pair<GLint, GLint> &xyOffset : xyOffsets)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, xyOffset.first, xyOffset.second, 1, 1, GL_RGBA,
                        GL_UNSIGNED_BYTE, kRandomColor.data());
    }

    // Update the entire texture
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTexWidth, kTexHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                    fullTextureData.data());

    setUpProgram();
    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth() - 1, getWindowHeight() - 1, kFullTextureColor);
}

// Test that repeated calls to glTexSubImage2D with superseding updates works
TEST_P(Texture2DTest, ManySupersedingTextureUpdates)
{
    constexpr uint32_t kTexWidth  = 3840;
    constexpr uint32_t kTexHeight = 2160;
    constexpr uint32_t kBpp       = 4;
    std::vector<GLubyte> data(kTexWidth * kTexHeight * kBpp, 0);

    // Create the texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kTexWidth, kTexHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    EXPECT_GL_ERROR(GL_NO_ERROR);

    // Make a large number of superseding updates
    for (uint32_t width = kTexWidth / 2, height = kTexHeight / 2;
         width < kTexWidth && height < kTexHeight; width++, height++)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                        data.data());
    }

    // Upload different color to the whole texture thus superseding all prior updates.
    std::vector<GLubyte> supersedingData(kTexWidth * kTexHeight * kBpp, 128);
    constexpr GLColor kGray = GLColor(128u, 128u, 128u, 128u);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTexWidth, kTexHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                    supersedingData.data());

    setUpProgram();
    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, kGray);
}

TEST_P(Texture2DTest, DefineMultipleLevelsWithoutMipmapping)
{
    setUpProgram();

    constexpr size_t kImageSize = 256;
    std::array<GLColor, kImageSize * kImageSize> kMipColors[2];

    std::fill(kMipColors[0].begin(), kMipColors[0].end(), GLColor::red);
    std::fill(kMipColors[1].begin(), kMipColors[1].end(), GLColor::green);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kImageSize, kImageSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 kMipColors[0].data());
    EXPECT_GL_NO_ERROR();

    // Draw so the image is created.
    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);

    // Define level 1 of the texture.
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, kImageSize, kImageSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 kMipColors[1].data());
    EXPECT_GL_NO_ERROR();

    // Draw again.
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[0][0]);
}

// Test drawing with two texture types, to trigger an ANGLE bug in validation
TEST_P(TextureCubeTest, CubeMapBug)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureCube);
    EXPECT_GL_ERROR(GL_NO_ERROR);

    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    glUniform1i(mTextureCubeUniformLocation, 1);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
}

// Test drawing with two texture types accessed from the same shader and check that the result of
// drawing is correct.
TEST_P(TextureCubeTest, CubeMapDraw)
{
    GLubyte texData[4];
    texData[0] = 0;
    texData[1] = 60;
    texData[2] = 0;
    texData[3] = 255;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, texData);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureCube);
    texData[1] = 120;
    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    texData);
    EXPECT_GL_ERROR(GL_NO_ERROR);

    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    glUniform1i(mTextureCubeUniformLocation, 1);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();

    int px = getWindowWidth() - 1;
    int py = 0;
    EXPECT_PIXEL_NEAR(px, py, 0, 180, 0, 255, 2);
}

TEST_P(Sampler2DAsFunctionParameterTest, Sampler2DAsFunctionParameter)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    GLubyte texData[4];
    texData[0] = 0;
    texData[1] = 128;
    texData[2] = 0;
    texData[3] = 255;
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_NEAR(0, 0, 0, 128, 0, 255, 2);
}

// Test drawing with two textures passed to the shader in a sampler array.
TEST_P(SamplerArrayTest, SamplerArrayDraw)
{
    testSamplerArrayDraw();
}

// Test drawing with two textures passed to the shader in a sampler array which is passed to a
// user-defined function in the shader.
TEST_P(SamplerArrayAsFunctionParameterTest, SamplerArrayAsFunctionParameter)
{
    // TODO: Diagnose and fix. http://anglebug.com/2955
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsAndroid());

    testSamplerArrayDraw();
}

// Copy of a test in conformance/textures/texture-mips, to test generate mipmaps
TEST_P(Texture2DTestWithDrawScale, MipmapsTwice)
{
    int px = getWindowWidth() / 2;
    int py = getWindowHeight() / 2;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    std::vector<GLColor> pixelsRed(16u * 16u, GLColor::red);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsRed.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    glUniform2f(mDrawScaleUniformLocation, 0.0625f, 0.0625f);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::red);

    std::vector<GLColor> pixelsBlue(16u * 16u, GLColor::blue);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelsBlue.data());
    glGenerateMipmap(GL_TEXTURE_2D);

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::blue);

    std::vector<GLColor> pixelsGreen(16u * 16u, GLColor::green);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelsGreen.data());
    glGenerateMipmap(GL_TEXTURE_2D);

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::green);
}

// Test creating a FBO with a cube map render target, to test an ANGLE bug
// https://code.google.com/p/angleproject/issues/detail?id=849
TEST_P(TextureCubeTest, CubeMapFBO)
{
    // http://anglebug.com/3145
    ANGLE_SKIP_TEST_IF(IsFuchsia() && IsIntel() && IsVulkan());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureCube);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
                           mTextureCube, 0);

    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    EXPECT_GL_NO_ERROR();

    // Test clearing the six mip faces individually.
    std::array<GLColor, 6> faceColors = {{GLColor::red, GLColor::green, GLColor::blue,
                                          GLColor::yellow, GLColor::cyan, GLColor::magenta}};

    for (size_t faceIndex = 0; faceIndex < 6; ++faceIndex)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, mTextureCube, 0);

        Vector4 clearColorF = faceColors[faceIndex].toNormalizedVector();
        glClearColor(clearColorF.x(), clearColorF.y(), clearColorF.z(), clearColorF.w());
        glClear(GL_COLOR_BUFFER_BIT);

        EXPECT_PIXEL_COLOR_EQ(0, 0, faceColors[faceIndex]);
    }

    // Iterate the faces again to make sure the colors haven't changed.
    for (size_t faceIndex = 0; faceIndex < 6; ++faceIndex)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, mTextureCube, 0);
        EXPECT_PIXEL_COLOR_EQ(0, 0, faceColors[faceIndex])
            << "face color " << faceIndex << " shouldn't change";
    }
}

// Tests clearing a cube map with a scissor enabled.
TEST_P(TextureCubeTest, CubeMapFBOScissoredClear)
{
    // http://anglebug.com/3145
    ANGLE_SKIP_TEST_IF(IsFuchsia() && IsIntel() && IsVulkan());

    constexpr size_t kSize = 16;

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, kSize, kSize);

    GLTexture texcube;
    glBindTexture(GL_TEXTURE_CUBE_MAP, texcube);
    for (GLenum face = 0; face < 6; face++)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
    }
    ASSERT_GL_NO_ERROR();

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
                           texcube, 0);

    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    ASSERT_GL_NO_ERROR();

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    glEnable(GL_SCISSOR_TEST);
    glScissor(kSize / 2, 0, kSize / 2, kSize);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kSize / 2 + 1, 0, GLColor::green);

    ASSERT_GL_NO_ERROR();
}

// Test that glTexSubImage2D works properly when glTexStorage2DEXT has initialized the image with a
// default color.
TEST_P(Texture2DTest, TexStorage)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       !IsGLExtensionEnabled("GL_EXT_texture_storage"));

    int width  = getWindowWidth();
    int height = getWindowHeight();

    GLTexture tex2D;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex2D);

    // Fill with red
    std::vector<GLubyte> pixels(3 * 16 * 16);
    for (size_t pixelId = 0; pixelId < 16 * 16; ++pixelId)
    {
        pixels[pixelId * 3 + 0] = 255;
        pixels[pixelId * 3 + 1] = 0;
        pixels[pixelId * 3 + 2] = 0;
    }

    // ANGLE internally uses RGBA as the DirectX format for RGB images
    // therefore glTexStorage2DEXT initializes the image to a default color to get a consistent
    // alpha color. The data is kept in a CPU-side image and the image is marked as dirty.
    if (getClientMajorVersion() >= 3)
    {
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, 16, 16);
    }
    else
    {
        glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGB8, 16, 16);
    }

    // Initializes the color of the upper-left 8x8 pixels, leaves the other pixels untouched.
    // glTexSubImage2D should take into account that the image is dirty.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    setUpProgram();

    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(width / 4, height / 4, 255, 0, 0, 255);

    // Validate that the region of the texture without data has an alpha of 1.0
    angle::GLColor pixel = ReadColor(3 * width / 4, 3 * height / 4);
    EXPECT_EQ(255, pixel.A);
}

// Test that glTexSubImage2D combined with a PBO works properly when glTexStorage2DEXT has
// initialized the image with a default color.
TEST_P(Texture2DTest, TexStorageWithPBO)
{
    // http://anglebug.com/4126
    ANGLE_SKIP_TEST_IF(IsOSX() && IsOpenGL());

    // http://anglebug.com/5081
    ANGLE_SKIP_TEST_IF(IsWindows() && IsNVIDIA() && IsOpenGL());

    // http://anglebug.com/5651
    ANGLE_SKIP_TEST_IF(IsLinux() && IsNVIDIA() && IsOpenGL());

    // http://anglebug.com/5097
    ANGLE_SKIP_TEST_IF(IsLinux() && IsOpenGL() && IsTSan());

    if (getClientMajorVersion() < 3)
    {
        ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_storage"));
        ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_NV_pixel_buffer_object"));
    }

    const int width          = getWindowWidth();
    const int height         = getWindowHeight();
    const size_t pixelCount  = width * height;
    const int componentCount = 3;

    GLTexture tex2D;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex2D);

    // Fill with red
    std::vector<GLubyte> pixels(componentCount * pixelCount);
    for (size_t pixelId = 0; pixelId < pixelCount; ++pixelId)
    {
        pixels[pixelId * componentCount + 0] = 255;
        pixels[pixelId * componentCount + 1] = 0;
        pixels[pixelId * componentCount + 2] = 0;
    }

    // Read 16x16 region from red backbuffer to PBO
    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, componentCount * pixelCount, pixels.data(),
                 GL_STATIC_DRAW);

    // ANGLE internally uses RGBA as the DirectX format for RGB images
    // therefore glTexStorage2DEXT initializes the image to a default color to get a consistent
    // alpha color. The data is kept in a CPU-side image and the image is marked as dirty.
    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGB8, width, height);

    // Initializes the color of the upper-left quadrant of pixels, leaves the other pixels
    // untouched. glTexSubImage2D should take into account that the image is dirty.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RGB, GL_UNSIGNED_BYTE,
                    nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    setUpProgram();

    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    glDeleteBuffers(1, &pbo);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(3 * width / 4, 3 * height / 4, 0, 0, 0, 255);
    EXPECT_PIXEL_EQ(width / 4, height / 4, 255, 0, 0, 255);
}

// Test that glTexSubImage2D combined with a PBO works properly after deleting the PBO
// and drawing with the texture
// Pseudo code for the follow test:
// 1. Upload PBO to mTexture2D
// 2. Delete PBO
// 3. Draw with otherTexture (x5)
// 4. Draw with mTexture2D
// 5. Validate color output
TEST_P(Texture2DTest, PBOWithMultipleDraws)
{
    if (getClientMajorVersion() < 3)
    {
        ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_storage"));
        ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_NV_pixel_buffer_object"));
    }

    const GLuint width            = getWindowWidth();
    const GLuint height           = getWindowHeight();
    const GLuint windowPixelCount = width * height;
    std::vector<GLColor> pixelsRed(windowPixelCount, GLColor::red);
    std::vector<GLColor> pixelsGreen(windowPixelCount, GLColor::green);

    // Create secondary draw that does not use mTexture
    const char *vertexShaderSource   = getVertexShaderSource();
    const char *fragmentShaderSource = getFragmentShaderSource();
    ANGLE_GL_PROGRAM(otherProgram, vertexShaderSource, fragmentShaderSource);

    GLint uniformLoc = glGetUniformLocation(otherProgram, getTextureUniformName());
    ASSERT_NE(-1, uniformLoc);
    glUseProgram(0);

    // Create secondary Texture to draw with
    GLTexture otherTexture;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, otherTexture);
    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                    pixelsRed.data());
    ASSERT_GL_NO_ERROR();

    // Setup primary Texture
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    ASSERT_GL_NO_ERROR();

    // Setup PBO
    GLuint pbo = 0;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, pixelsGreen.size() * 4u, pixelsGreen.data(),
                 GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    // Write PBO to mTexture
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    ASSERT_GL_NO_ERROR();
    // Delete PBO as ANGLE should be properly handling refcount of this buffer
    glDeleteBuffers(1, &pbo);
    pixelsGreen.clear();

    // Do 5 draws not involving primary texture that the PBO updated
    glUseProgram(otherProgram);
    glUniform1i(uniformLoc, 0);
    glBindTexture(GL_TEXTURE_2D, otherTexture);
    drawQuad(otherProgram, "position", 0.5f);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    glUseProgram(otherProgram);
    glUniform1i(uniformLoc, 0);
    glBindTexture(GL_TEXTURE_2D, otherTexture);
    drawQuad(otherProgram, "position", 0.5f);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    glUseProgram(otherProgram);
    glUniform1i(uniformLoc, 0);
    glBindTexture(GL_TEXTURE_2D, otherTexture);
    drawQuad(otherProgram, "position", 0.5f);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    glUseProgram(otherProgram);
    glUniform1i(uniformLoc, 0);
    glBindTexture(GL_TEXTURE_2D, otherTexture);
    drawQuad(otherProgram, "position", 0.5f);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    ASSERT_GL_NO_ERROR();

    std::vector<GLColor> output(windowPixelCount, GLColor::black);
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 output.data());
    EXPECT_EQ(pixelsRed, output);

    setUpProgram();
    // Draw using PBO updated texture
    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    std::vector<GLColor> actual(windowPixelCount, GLColor::black);
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actual.data());
    // Value should be green as it was updated during PBO transfer to mTexture
    std::vector<GLColor> expected(windowPixelCount, GLColor::green);
    EXPECT_EQ(expected, actual);
}

// Test that glTexSubImage2D combined with a PBO works properly. PBO has all pixels as red
// except the middle one being green.
TEST_P(Texture2DTest, TexStorageWithPBOMiddlePixelDifferent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_NV_pixel_buffer_object"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_storage"));

    int width  = getWindowWidth();
    int height = getWindowHeight();

    GLTexture tex2D;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex2D);

    std::vector<GLubyte> pixels(3 * 16 * 16);

    // Initialize texture with default black color.
    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGB8, 16, 16);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 16, 16, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // Fill PBO's data with red, with middle one as green
    for (size_t pixelId = 0; pixelId < 16 * 16; ++pixelId)
    {
        if (pixelId == 8 * 7 + 7)
        {
            pixels[pixelId * 3 + 0] = 0;
            pixels[pixelId * 3 + 1] = 255;
            pixels[pixelId * 3 + 2] = 0;
        }
        else
        {
            pixels[pixelId * 3 + 0] = 255;
            pixels[pixelId * 3 + 1] = 0;
            pixels[pixelId * 3 + 2] = 0;
        }
    }

    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 3 * 16 * 16, pixels.data(), GL_STATIC_DRAW);

    // Update the color of the texture's upper-left 8x8 pixels, leaves the other pixels untouched.
    // glTexSubImage2D should take into account that the image is dirty.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    setUpProgram();

    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    glDeleteBuffers(1, &pbo);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(3 * width / 4, 3 * height / 4, 0, 0, 0, 255);
    EXPECT_PIXEL_EQ(width / 4, height / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(width / 2 - 1, height / 2 - 1, 0, 255, 0, 255);
}

// Test that glTexSubImage2D combined with a PBO works properly when glTexImage2D has
// initialized the image with a luminance color
TEST_P(Texture2DTest, TexImageWithLuminancePBO)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_NV_pixel_buffer_object"));

    int width  = getWindowWidth();
    int height = getWindowHeight();

    GLTexture tex2D;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex2D);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 16, 16, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                 nullptr);

    // Fill PBO with white, with middle one as grey
    std::vector<GLubyte> pixels(16 * 16);
    for (size_t pixelId = 0; pixelId < 16 * 16; ++pixelId)
    {
        if (pixelId == 8 * 7 + 7)
        {
            pixels[pixelId] = 128;
        }
        else
        {
            pixels[pixelId] = 255;
        }
    }

    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 16 * 16, pixels.data(), GL_STATIC_DRAW);

    // Initializes the color of the upper-left 8x8 pixels, leaves the other pixels untouched.
    // glTexSubImage2D should take into account that the image is dirty.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    setUpProgram();

    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    glDeleteBuffers(1, &pbo);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(width / 4, height / 4, 255, 255, 255, 255);
    EXPECT_PIXEL_NEAR(width / 2 - 1, height / 2 - 1, 128, 128, 128, 255, 1);
}

// Test that glTexSubImage2D combined with a PBO works properly when glTexStorage2DEXT has
// initialized the image with a RGB656 color
TEST_P(Texture2DTest, TexImageWithRGB565PBO)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_NV_pixel_buffer_object"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_storage"));

    int width  = getWindowWidth();
    int height = getWindowHeight();

    GLTexture tex2D;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex2D);

    // Fill PBO with red, with middle one as green
    std::vector<GLushort> pixels(16 * 16);
    for (size_t pixelId = 0; pixelId < 16 * 16; ++pixelId)
    {
        if (pixelId == 8 * 7 + 8)
        {
            pixels[pixelId] = 0x7E0;
        }
        else
        {
            pixels[pixelId] = 0xF800;
        }
    }

    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 2 * 16 * 16, pixels.data(), GL_STATIC_DRAW);

    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGB565, 16, 16);

    // Initializes the color of the upper-left 8x8 pixels, leaves the other pixels untouched.
    // glTexSubImage2D should take into account that the image is dirty.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                    reinterpret_cast<void *>(2));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    setUpProgram();

    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    glDeleteBuffers(1, &pbo);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(width / 4, height / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(width / 2 - 1, height / 2 - 1, 0, 255, 0, 255);
}

// Test that glTexSubImage2D combined with a PBO works properly when glTexStorage2DEXT has
// initialized the image with a RGBA4444 color
TEST_P(Texture2DTest, TexImageWithRGBA4444PBO)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_NV_pixel_buffer_object"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_storage"));

    int width  = getWindowWidth();
    int height = getWindowHeight();

    GLTexture tex2D;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex2D);

    // Fill PBO with red, with middle one as green
    std::vector<GLushort> pixels(16 * 16);
    for (size_t pixelId = 0; pixelId < 16 * 16; ++pixelId)
    {
        if (pixelId == 8 * 7 + 8)
        {
            pixels[pixelId] = 0xF0F;
        }
        else
        {
            pixels[pixelId] = 0xF00F;
        }
    }

    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 2 * 16 * 16, pixels.data(), GL_STATIC_DRAW);

    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA4, 16, 16);

    // Initializes the color of the upper-left 8x8 pixels, leaves the other pixels untouched.
    // glTexSubImage2D should take into account that the image is dirty.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4,
                    reinterpret_cast<void *>(2));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    setUpProgram();

    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    glDeleteBuffers(1, &pbo);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(width / 4, height / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(width / 2 - 1, height / 2 - 1, 0, 255, 0, 255);
}

// Test that glTexSubImage2D combined with a PBO works properly when glTexStorage2DEXT has
// initialized the image with a RGBA5551 color
TEST_P(Texture2DTest, TexImageWithRGBA5551PBO)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_NV_pixel_buffer_object"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_storage"));

    int width  = getWindowWidth();
    int height = getWindowHeight();

    GLTexture tex2D;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex2D);

    // Fill PBO with red, with middle one as green
    std::vector<GLushort> pixels(16 * 16);
    for (size_t pixelId = 0; pixelId < 16 * 16; ++pixelId)
    {
        if (pixelId == 8 * 7 + 7)
        {
            pixels[pixelId] = 0x7C1;
        }
        else
        {
            pixels[pixelId] = 0xF801;
        }
    }

    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 2 * 16 * 16, pixels.data(), GL_STATIC_DRAW);

    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGB5_A1, 16, 16);

    // Initializes the color of the upper-left 8x8 pixels, leaves the other pixels untouched.
    // glTexSubImage2D should take into account that the image is dirty.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    setUpProgram();

    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    glDeleteBuffers(1, &pbo);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(width / 4, height / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(width / 2 - 1, height / 2 - 1, 0, 255, 0, 255);
}

// Test that glTexSubImage2D from a PBO respects GL_UNPACK_ROW_LENGTH.
TEST_P(Texture2DTest, TexImageUnpackRowLengthPBO)
{
    if (getClientMajorVersion() < 3)
    {
        ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_storage"));
        ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_NV_pixel_buffer_object"));
        ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_unpack_subimage"));
    }

    const int width      = getWindowWidth() / 2;
    const int height     = getWindowHeight();
    const int rowLength  = getWindowWidth();
    const int bufferSize = rowLength * height;

    GLTexture tex2D;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex2D);

    std::vector<GLColor> pixels(bufferSize);
    for (int y = 0; y < rowLength; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            pixels[y * rowLength + x] =
                x < width ? (y < height / 2 ? GLColor::green : GLColor::blue) : GLColor::red;
        }
    }

    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, bufferSize * sizeof(GLColor), pixels.data(),
                 GL_STATIC_DRAW);

    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

    // Initializes the texture from width x height of the PBO.
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    setUpProgram();

    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    glDeleteBuffers(1, &pbo);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, height - 1, GLColor::blue);
}

// Test if the KHR debug label is set and passed to D3D correctly using glCopyTexImage2D.
TEST_P(Texture2DTest, TextureKHRDebugLabelWithCopyTexImage2D)
{
    GLTexture texture2D;
    glBindTexture(GL_TEXTURE_2D, texture2D);

    // Create a texture and copy into, to initialize storage object.
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 32, 32, 0);

    // Set KHR Debug Label.
    std::string label = "TestKHR.DebugLabel";
    glObjectLabelKHR(GL_TEXTURE, texture2D, -1, label.c_str());

    std::vector<char> labelBuf(label.length() + 1);
    GLsizei labelLengthBuf = 0;

    glGetObjectLabelKHR(GL_TEXTURE, texture2D, static_cast<GLsizei>(labelBuf.size()),
                        &labelLengthBuf, labelBuf.data());

    EXPECT_EQ(static_cast<GLsizei>(label.length()), labelLengthBuf);
    EXPECT_STREQ(label.c_str(), labelBuf.data());

    // Delete the texture.
    texture2D.reset();
    EXPECT_GL_NO_ERROR();

    glObjectLabelKHR(GL_TEXTURE, texture2D, -1, label.c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glGetObjectLabelKHR(GL_TEXTURE, texture2D, static_cast<GLsizei>(labelBuf.size()),
                        &labelLengthBuf, labelBuf.data());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test to call labeling API before the storage texture is created.
TEST_P(Texture2DTest, CallKHRDebugLabelBeforeTexStorageCreation)
{
    GLTexture texture2D;
    glBindTexture(GL_TEXTURE_2D, texture2D);

    // Set label before texture storage creation.
    std::string label = "TestKHR.DebugLabel";
    glObjectLabelKHR(GL_TEXTURE, texture2D, -1, label.c_str());

    // Create a texture and copy into, to initialize storage object.
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 32, 32, 0);

    std::vector<char> labelBuf(label.length() + 1);
    GLsizei labelLengthBuf = 0;

    glGetObjectLabelKHR(GL_TEXTURE, texture2D, static_cast<GLsizei>(labelBuf.size()),
                        &labelLengthBuf, labelBuf.data());

    EXPECT_EQ(static_cast<GLsizei>(label.length()), labelLengthBuf);
    EXPECT_STREQ(label.c_str(), labelBuf.data());

    // Delete the texture.
    texture2D.reset();
    EXPECT_GL_NO_ERROR();

    glObjectLabelKHR(GL_TEXTURE, texture2D, -1, label.c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glGetObjectLabelKHR(GL_TEXTURE, texture2D, static_cast<GLsizei>(labelBuf.size()),
                        &labelLengthBuf, labelBuf.data());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that glTexSubImage2D combined with a PBO works properly when glTexStorage2D has
// initialized the image with a depth-only format.
TEST_P(Texture2DTestES3, TexImageWithDepthPBO)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_NV_pixel_buffer_object"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_storage"));

    // http://anglebug.com/5315
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsOSX());

    constexpr GLsizei kSize = 4;

    // Set up the framebuffer.
    GLTexture colorTexture;
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    GLTexture depthTexture;
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, kSize, kSize);
    ASSERT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    ASSERT_GL_NO_ERROR();

    // Clear depth to 0, ensuring the texture's image is allocated.
    glClearDepthf(0);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);

    // Fill depth with 1.0f.
    std::vector<GLushort> pixels(kSize * kSize);
    for (size_t pixelId = 0; pixelId < pixels.size(); ++pixelId)
    {
        pixels[pixelId] = 0xFFFF;
    }

    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, pixels.size() * sizeof(pixels[0]), pixels.data(),
                 GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    // Upload PBO data.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kSize, kSize, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,
                    nullptr);

    // If depth is not set to 1, rendering would fail.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Draw red
    ANGLE_GL_PROGRAM(drawRed, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    drawQuad(drawRed, essl1_shaders::PositionAttrib(), 0.95f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, GLColor::red);
}

// Test that sampling stencil from a DEPTH24_STENCIL8 texture works.
TEST_P(Texture2DTestES31, TexSampleStencilFromDepth24Stencil8)
{
    TestSampleStencilFromDepthStencil(GL_DEPTH24_STENCIL8, false);
    TestSampleStencilFromDepthStencil(GL_DEPTH24_STENCIL8, true);
}

// Test that sampling stencil from a DEPTH32F_STENCIL8 texture works.
TEST_P(Texture2DTestES31, TexSampleStencilFromDepth32fStencil8)
{
    TestSampleStencilFromDepthStencil(GL_DEPTH32F_STENCIL8, false);
    TestSampleStencilFromDepthStencil(GL_DEPTH32F_STENCIL8, true);
}

// Test that glTexSubImage2D combined with a PBO works properly when glTexStorage2D has
// initialized the image with a stencil-only format.
TEST_P(Texture2DTestES3, TexImageWithStencilPBO)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_NV_pixel_buffer_object"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_storage"));

    // http://anglebug.com/5315
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsOSX());

    // http://anglebug.com/5317
    ANGLE_SKIP_TEST_IF(IsWindows() && IsD3D());

    constexpr GLsizei kSize = 4;

    // Set up the framebuffer.
    GLTexture colorTexture;
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    GLTexture stencilTexture;
    glBindTexture(GL_TEXTURE_2D, stencilTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_STENCIL_INDEX8, kSize, kSize);
    ASSERT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, stencilTexture, 0);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    ASSERT_GL_NO_ERROR();

    // Clear stencil to 0, ensuring the texture's image is allocated.
    glClearStencil(0);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);

    // Fill stencil with 0x4E
    std::vector<GLubyte> pixels(kSize * kSize);
    for (size_t pixelId = 0; pixelId < pixels.size(); ++pixelId)
    {
        pixels[pixelId] = 0x4E;
    }

    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, pixels.size() * sizeof(pixels[0]), pixels.data(),
                 GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    // Upload PBO data.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kSize, kSize, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE,
                    nullptr);
    ASSERT_GL_NO_ERROR();

    // If stencil is not set to 0x4E, rendering would fail.
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 0x4E, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilMask(0xFF);

    // Draw red
    ANGLE_GL_PROGRAM(drawRed, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    drawQuad(drawRed, essl1_shaders::PositionAttrib(), 0.95f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, GLColor::red);
}

// Test that glTexSubImage2D combined with a PBO works properly when glTexStorage2D has
// initialized the image with a depth/stencil format.
TEST_P(Texture2DTestES3, TexImageWithDepthStencilPBO)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_NV_pixel_buffer_object"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_storage"));

    // http://anglebug.com/5313
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsAndroid());

    // http://anglebug.com/5315
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsOSX());

    // http://anglebug.com/5317
    ANGLE_SKIP_TEST_IF(IsWindows() && IsD3D());

    constexpr GLsizei kSize = 4;

    // Set up the framebuffer.
    GLTexture colorTexture;
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    GLTexture depthStencilTexture;
    glBindTexture(GL_TEXTURE_2D, depthStencilTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, kSize, kSize);
    ASSERT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                           depthStencilTexture, 0);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    ASSERT_GL_NO_ERROR();

    // Clear depth and stencil to 0, ensuring the texture's image is allocated.
    glClearDepthf(0);
    glClearStencil(0);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);

    // Fill depth with 1.0f and stencil with 0xD5
    std::vector<GLuint> pixels(kSize * kSize);
    for (size_t pixelId = 0; pixelId < pixels.size(); ++pixelId)
    {
        pixels[pixelId] = 0xFFFFFFD5;
    }

    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, pixels.size() * sizeof(pixels[0]), pixels.data(),
                 GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    // Upload PBO data.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kSize, kSize, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
                    nullptr);

    // If depth is not set to 1, rendering would fail.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // If stencil is not set to 0xD5, rendering would fail.
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 0xD5, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilMask(0xFF);

    // Draw red
    ANGLE_GL_PROGRAM(drawRed, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    drawQuad(drawRed, essl1_shaders::PositionAttrib(), 0.95f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, GLColor::red);
}

// Test functionality of GL_ANGLE_yuv_internal_format with min/mag filters
// set to nearest and linear modes.
TEST_P(Texture2DTestES3YUV, TexStorage2DYuvFilterModes)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_yuv_internal_format"));

    // Create YUV texture
    GLTexture yuvTexture;
    GLubyte yuvColor[]         = {40, 40, 40, 40, 40, 40, 40, 40, 240, 109, 240, 109};
    GLubyte expectedRgbColor[] = {0, 0, 255, 255};
    createImmutableTexture2D(yuvTexture, 2, 4, GL_G8_B8R8_2PLANE_420_UNORM_ANGLE,
                             GL_G8_B8R8_2PLANE_420_UNORM_ANGLE, GL_UNSIGNED_BYTE, 1, yuvColor);

    // Default is nearest filter mode
    verifyResults2D(yuvTexture, expectedRgbColor);
    ASSERT_GL_NO_ERROR();

    // Enable linear filter mode
    glBindTexture(GL_TEXTURE_2D, yuvTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    verifyResults2D(yuvTexture, expectedRgbColor);
    ASSERT_GL_NO_ERROR();

    const int windowHeight = getWindowHeight();
    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor::blue, 1);
    EXPECT_PIXEL_COLOR_NEAR(0, windowHeight - 1, GLColor::blue, 1);
    EXPECT_PIXEL_COLOR_NEAR(0, windowHeight / 2, GLColor::blue, 1);
}

// Test functionality of GL_ANGLE_yuv_internal_format while cycling through RGB and YUV sources
TEST_P(Texture2DTestES3, TexStorage2DCycleThroughYuvAndRgbSources)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_yuv_internal_format"));

    // Create YUV texture
    GLTexture yuvTexture;
    GLubyte yuvColor[6]         = {40, 40, 40, 40, 240, 109};
    GLubyte expectedRgbColor[4] = {0, 0, 255, 255};
    createImmutableTexture2D(yuvTexture, 2, 2, GL_G8_B8R8_2PLANE_420_UNORM_ANGLE,
                             GL_G8_B8R8_2PLANE_420_UNORM_ANGLE, GL_UNSIGNED_BYTE, 1, yuvColor);

    // Create RGBA texture
    GLTexture rgbaTexture;
    GLubyte rgbaColor[4] = {0, 0, 255, 255};
    createImmutableTexture2D(rgbaTexture, 1, 1, GL_RGBA, GL_RGBA8, GL_UNSIGNED_BYTE, 1, rgbaColor);

    // Cycle through source textures
    // RGBA source
    verifyResults2D(rgbaTexture, rgbaColor);
    ASSERT_GL_NO_ERROR();

    // YUV source
    verifyResults2D(yuvTexture, expectedRgbColor);
    ASSERT_GL_NO_ERROR();

    // RGBA source
    verifyResults2D(rgbaTexture, rgbaColor);
    ASSERT_GL_NO_ERROR();
}

// Test functionality of GL_ANGLE_yuv_internal_format with large number of YUV sources
TEST_P(Texture2DTestES3, TexStorage2DLargeYuvTextureCount)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_yuv_internal_format"));

    constexpr uint32_t kTextureCount = 16;

    // Create YUV texture
    GLTexture yuvTexture[kTextureCount];
    for (uint32_t i = 0; i < kTextureCount; i++)
    {
        // Create 2 plane YCbCr 420 texture
        createImmutableTexture2D(yuvTexture[i], 2, 2, GL_G8_B8R8_2PLANE_420_UNORM_ANGLE,
                                 GL_G8_B8R8_2PLANE_420_UNORM_ANGLE, GL_UNSIGNED_BYTE, 1, nullptr);
    }

    // Cycle through YUV source textures
    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);

    for (uint32_t i = 0; i < kTextureCount; i++)
    {
        glBindTexture(GL_TEXTURE_2D, yuvTexture[i]);
        drawQuad(mProgram, "position", 0.5f);
        ASSERT_GL_NO_ERROR();
    }
}

// Test functionality of GL_ANGLE_yuv_internal_format with simultaneous use of multiple YUV sources
TEST_P(Texture2DTestES3, TexStorage2DSimultaneousUseOfMultipleYuvSourcesNoData)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_yuv_internal_format"));

    // Create YUV texture
    // Create 2 plane YCbCr 420 texture
    GLTexture twoPlaneYuvTexture;
    createImmutableTexture2D(twoPlaneYuvTexture, 2, 2, GL_G8_B8R8_2PLANE_420_UNORM_ANGLE,
                             GL_G8_B8R8_2PLANE_420_UNORM_ANGLE, GL_UNSIGNED_BYTE, 1, nullptr);

    // Create 3 plane YCbCr 420 texture
    GLTexture threePlaneYuvTexture;
    createImmutableTexture2D(threePlaneYuvTexture, 2, 2, GL_G8_B8_R8_3PLANE_420_UNORM_ANGLE,
                             GL_G8_B8_R8_3PLANE_420_UNORM_ANGLE, GL_UNSIGNED_BYTE, 1, nullptr);

    // Cycle through YUV source textures
    // Create program with 2 samplers
    const char *vertexShaderSource   = getVertexShaderSource();
    const char *fragmentShaderSource = R"(#version 300 es
precision highp float;
uniform sampler2D tex0;
uniform sampler2D tex1;
in vec2 texcoord;
out vec4 fragColor;

void main()
{
    vec4 color0 = texture(tex0, texcoord);
    vec4 color1 = texture(tex1, texcoord);
    fragColor = color0 + color1;
})";

    ANGLE_GL_PROGRAM(twoSamplersProgram, vertexShaderSource, fragmentShaderSource);
    glUseProgram(twoSamplersProgram);
    GLint tex0Location = glGetUniformLocation(twoSamplersProgram, "tex0");
    ASSERT_NE(-1, tex0Location);
    GLint tex1Location = glGetUniformLocation(twoSamplersProgram, "tex1");
    ASSERT_NE(-1, tex1Location);

    glUniform1i(tex0Location, 0);
    glUniform1i(tex1Location, 1);

    // Bind 2 plane YUV source
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, twoPlaneYuvTexture);
    ASSERT_GL_NO_ERROR();

    // Bind 3 plane YUV source
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, threePlaneYuvTexture);
    ASSERT_GL_NO_ERROR();

    drawQuad(twoSamplersProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    // Switch active texture index and draw again
    // Bind 2 plane YUV source
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, twoPlaneYuvTexture);
    ASSERT_GL_NO_ERROR();

    // Bind 3 plane YUV source
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, threePlaneYuvTexture);
    ASSERT_GL_NO_ERROR();

    drawQuad(twoSamplersProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
}

// Test functional of GL_ANGLE_yuv_internal_format while cycling through YUV sources
TEST_P(Texture2DTestES3, TexStorage2DCycleThroughYuvSourcesNoData)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_yuv_internal_format"));

    // Create YUV texture
    // Create 2 plane YCbCr 420 texture
    GLTexture twoPlaneYuvTexture;
    createImmutableTexture2D(twoPlaneYuvTexture, 2, 2, GL_G8_B8R8_2PLANE_420_UNORM_ANGLE,
                             GL_G8_B8R8_2PLANE_420_UNORM_ANGLE, GL_UNSIGNED_BYTE, 1, nullptr);

    // Create 3 plane YCbCr 420 texture
    GLTexture threePlaneYuvTexture;
    createImmutableTexture2D(threePlaneYuvTexture, 2, 2, GL_G8_B8_R8_3PLANE_420_UNORM_ANGLE,
                             GL_G8_B8_R8_3PLANE_420_UNORM_ANGLE, GL_UNSIGNED_BYTE, 1, nullptr);

    // Cycle through YUV source textures
    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);

    // 2 plane YUV source
    glBindTexture(GL_TEXTURE_2D, twoPlaneYuvTexture);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    // 3 plane YUV source
    glBindTexture(GL_TEXTURE_2D, threePlaneYuvTexture);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    // 2 plane YUV source
    glBindTexture(GL_TEXTURE_2D, twoPlaneYuvTexture);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
}

// Tests CopySubImage for float formats
TEST_P(Texture2DTest, CopySubImageFloat_R_R)
{
    testFloatCopySubImage(1, 1);
}

TEST_P(Texture2DTest, CopySubImageFloat_RG_R)
{
    testFloatCopySubImage(2, 1);
}

TEST_P(Texture2DTest, CopySubImageFloat_RG_RG)
{
    testFloatCopySubImage(2, 2);
}

TEST_P(Texture2DTest, CopySubImageFloat_RGB_R)
{
    testFloatCopySubImage(3, 1);
}

TEST_P(Texture2DTest, CopySubImageFloat_RGB_RG)
{
    testFloatCopySubImage(3, 2);
}

TEST_P(Texture2DTest, CopySubImageFloat_RGB_RGB)
{
    // TODO(cwallez): Fix on Linux Intel drivers (http://anglebug.com/1346)
    ANGLE_SKIP_TEST_IF(IsIntel() && IsLinux() && IsOpenGL());

    testFloatCopySubImage(3, 3);
}

TEST_P(Texture2DTest, CopySubImageFloat_RGBA_R)
{
    testFloatCopySubImage(4, 1);
}

TEST_P(Texture2DTest, CopySubImageFloat_RGBA_RG)
{
    testFloatCopySubImage(4, 2);
}

TEST_P(Texture2DTest, CopySubImageFloat_RGBA_RGB)
{
    testFloatCopySubImage(4, 3);
}

TEST_P(Texture2DTest, CopySubImageFloat_RGBA_RGBA)
{
    testFloatCopySubImage(4, 4);
}

// Port of
// https://www.khronos.org/registry/webgl/conformance-suites/1.0.3/conformance/textures/texture-npot.html
// Run against GL_ALPHA/UNSIGNED_BYTE format, to ensure that D3D11 Feature Level 9_3 correctly
// handles GL_ALPHA
TEST_P(Texture2DTest, TextureNPOT_GL_ALPHA_UBYTE)
{
    const int npotTexSize = 5;
    const int potTexSize  = 4;  // Should be less than npotTexSize
    GLTexture tex2D;

    if (IsGLExtensionEnabled("GL_OES_texture_npot"))
    {
        // This test isn't applicable if texture_npot is enabled
        return;
    }

    setUpProgram();

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // Default unpack alignment is 4. The values of 'pixels' below needs it to be 1.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex2D);

    const std::vector<GLubyte> pixels(1 * npotTexSize * npotTexSize, 64);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Check that an NPOT texture not on level 0 generates INVALID_VALUE
    glTexImage2D(GL_TEXTURE_2D, 1, GL_ALPHA, npotTexSize, npotTexSize, 0, GL_ALPHA,
                 GL_UNSIGNED_BYTE, pixels.data());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Check that an NPOT texture on level 0 succeeds
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, npotTexSize, npotTexSize, 0, GL_ALPHA,
                 GL_UNSIGNED_BYTE, pixels.data());
    EXPECT_GL_NO_ERROR();

    // Check that generateMipmap fails on NPOT
    glGenerateMipmap(GL_TEXTURE_2D);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Check that nothing is drawn if filtering is not correct for NPOT
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glClear(GL_COLOR_BUFFER_BIT);
    drawQuad(mProgram, "position", 1.0f);
    EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 0, 0, 0, 255);

    // NPOT texture with TEXTURE_MIN_FILTER not NEAREST or LINEAR should draw with 0,0,0,255
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glClear(GL_COLOR_BUFFER_BIT);
    drawQuad(mProgram, "position", 1.0f);
    EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 0, 0, 0, 255);

    // NPOT texture with TEXTURE_MIN_FILTER set to LINEAR should draw
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glClear(GL_COLOR_BUFFER_BIT);
    drawQuad(mProgram, "position", 1.0f);
    EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 0, 0, 0, 64);

    // Check that glTexImage2D for POT texture succeeds
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, potTexSize, potTexSize, 0, GL_ALPHA, GL_UNSIGNED_BYTE,
                 pixels.data());
    EXPECT_GL_NO_ERROR();

    // Check that generateMipmap for an POT texture succeeds
    glGenerateMipmap(GL_TEXTURE_2D);
    EXPECT_GL_NO_ERROR();

    // POT texture with TEXTURE_MIN_FILTER set to LINEAR_MIPMAP_LINEAR should draw
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glClear(GL_COLOR_BUFFER_BIT);
    drawQuad(mProgram, "position", 1.0f);
    EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 0, 0, 0, 64);
    EXPECT_GL_NO_ERROR();
}

// Test to ensure that glTexSubImage2D always accepts data for non-power-of-two subregions.
// ANGLE previously rejected this if GL_OES_texture_npot wasn't active, which is incorrect.
TEST_P(Texture2DTest, NPOTSubImageParameters)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    // Create an 8x8 (i.e. power-of-two) texture.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Supply a 3x3 (i.e. non-power-of-two) subimage to the texture.
    // This should always work, even if GL_OES_texture_npot isn't active.
    std::array<GLColor, 3 * 3> data;
    glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 3, 3, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

    EXPECT_GL_NO_ERROR();
}

// Regression test for https://crbug.com/1222516 to prevent integer overflow during validation.
TEST_P(Texture2DTest, SubImageValidationOverflow)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_NO_ERROR();

    glTexSubImage2D(GL_TEXTURE_2D, 0, -4, 0, 2147483647, 1, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, -4, 1, 2147483647, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that when a mutable texture is deleted, its corresponding pointer in the Vulkan backend,
// which is used for mutable texture flushing, is also deleted, and is not accessed by the new
// mutable texture after it.
TEST_P(Texture2DTest, MutableUploadThenDeleteThenMutableUpload)
{
    GLTexture texture1;
    glBindTexture(GL_TEXTURE_2D, texture1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 GLColor::red.data());
    texture1.reset();
    EXPECT_GL_NO_ERROR();

    GLTexture texture2;
    glBindTexture(GL_TEXTURE_2D, texture2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 GLColor::green.data());
    texture2.reset();
    EXPECT_GL_NO_ERROR();
}

// Test to ensure that glTexStorage3D accepts ASTC sliced 3D. https://crbug.com/1060012
TEST_P(Texture3DTestES3, ImmutableASTCSliced3D)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_KHR_texture_compression_astc_sliced_3d"));

    glBindTexture(GL_TEXTURE_3D, mTexture3D);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_COMPRESSED_RGBA_ASTC_4x4, 4, 4, 1);
    EXPECT_GL_NO_ERROR();
}

void FillLevel(GLint level,
               GLuint width,
               GLuint height,
               const GLColor &color,
               bool cubemap,
               bool subTex)
{
    std::vector<GLColor> pixels(width * height, color);
    std::vector<GLenum> targets;
    if (cubemap)
    {
        targets = {GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                   GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                   GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};
    }
    else
    {
        targets = {GL_TEXTURE_2D};
    }

    for (GLenum target : targets)
    {
        if (subTex)
        {
            glTexSubImage2D(target, level, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                            pixels.data());
        }
        else
        {
            glTexImage2D(target, level, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         pixels.data());
        }
    }
}

// This is part of tests that webgl_conformance_vulkan_passthrough_tests
// conformance/textures/misc/texture-size.html does
void Texture2DTest::testTextureSize(int testCaseIndex)
{
    std::array<GLColor, 6> kNewMipColors = {
        GLColor::green,  GLColor::red,     GLColor::blue,
        GLColor::yellow, GLColor::magenta, GLColor::cyan,
    };
    GLuint colorCount = 0;

    setUpProgram();

    constexpr char kVS[] =
        R"(precision highp float;
attribute vec4 position;
varying vec3 texcoord;
void main()
{
    gl_Position = position;
    texcoord = (position.xyz * 0.5) + 0.5;
}
)";
    constexpr char kFS[] =
        R"(precision mediump float;
uniform samplerCube tex;
varying vec3 texcoord;
void main()
{
    gl_FragColor = textureCube(tex, texcoord);
})";
    ANGLE_GL_PROGRAM(programCubeMap, kVS, kFS);
    GLint textureCubeUniformLocation = glGetUniformLocation(programCubeMap, "tex");
    ASSERT_NE(-1, textureCubeUniformLocation);
    ASSERT_GL_NO_ERROR();

    GLint max2DSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max2DSize);
    GLint maxCubeMapSize = 0;
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maxCubeMapSize);
    // Assuming 2048x2048xRGBA (22 mb with mips) will run on all WebGL platforms
    GLint max2DSquareSize = std::min(max2DSize, 2048);
    // I'd prefer this to be 2048 but that's 16 mb x 6 faces or 128 mb (with mips)
    // 1024 is 33.5 mb (with mips)
    maxCubeMapSize = std::min(maxCubeMapSize, 1024);
    ASSERT_GL_NO_ERROR();

    for (GLint size = 1; size <= max2DSize; size *= 2)
    {
        bool cubeMap     = testCaseIndex == 3;
        GLenum texTarget = cubeMap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
        GLuint program   = cubeMap ? programCubeMap : mProgram;
        GLint texWidth = 0, texHeight = 0;

        switch (testCaseIndex)
        {
            case 0:
                texWidth  = size;
                texHeight = 1;
                break;
            case 1:
                texWidth  = 1;
                texHeight = size;
                break;
            case 2:
            case 3:
                texWidth  = size;
                texHeight = size;
                break;
        }

        if (texWidth == texHeight && size > max2DSquareSize)
        {
            return;
        }

        if (cubeMap && size > maxCubeMapSize)
        {
            return;
        }

        GLTexture texture;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(texTarget, texture);
        FillLevel(0, texWidth, texHeight, kNewMipColors[colorCount], cubeMap, false);
        glTexParameteri(texTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(texTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(texTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(texTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        ASSERT_GL_NO_ERROR();

        glClear(GL_COLOR_BUFFER_BIT);
        ASSERT_GL_NO_ERROR();

        glUseProgram(program);
        if (cubeMap)
        {
            glUniform1i(textureCubeUniformLocation, 0);
        }
        else
        {
            glUniform1i(mTexture2DUniformLocation, 0);
        }

        drawQuad(program, "position", 1.0f);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, kNewMipColors[colorCount]);

        colorCount = (colorCount + 1) % kNewMipColors.size();
        FillLevel(0, texWidth, texHeight, kNewMipColors[colorCount], cubeMap, false);
        glGenerateMipmap(texTarget);
        ASSERT_GL_NO_ERROR();

        glTexParameteri(texTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glClear(GL_COLOR_BUFFER_BIT);
        drawQuad(program, "position", 1.0f);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, kNewMipColors[colorCount]);

        colorCount = (colorCount + 1) % kNewMipColors.size();
        FillLevel(0, texWidth, texHeight, kNewMipColors[colorCount], cubeMap, true);
        glGenerateMipmap(texTarget);

        glClear(GL_COLOR_BUFFER_BIT);
        drawQuad(program, "position", 1.0f);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, kNewMipColors[colorCount]);
    }
}

void Texture2DTest::testTextureSizeError()
{
    GLint max2DSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max2DSize);
    glActiveTexture(GL_TEXTURE0);
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    FillLevel(0, max2DSize, max2DSize, GLColor::red, false, false);
    GLenum err  = glGetError();
    bool passed = (err == GL_NO_ERROR || err == GL_OUT_OF_MEMORY);
    ASSERT_TRUE(passed);
}

// Permutation 0 of testTextureSize.
TEST_P(Texture2DTest, TextureSizeCase0)
{
    testTextureSize(0);
}

// Permutation 1 of testTextureSize.
TEST_P(Texture2DTest, TextureSizeCase1)
{
    testTextureSize(1);
}

// Permutation 2 of testTextureSize.
TEST_P(Texture2DTest, TextureSizeCase2)
{
    testTextureSize(2);
}

// Permutation 3 of testTextureSize.
TEST_P(Texture2DTest, TextureSizeCase3)
{
    testTextureSize(3);
}

// Test allocating a very large texture
TEST_P(Texture2DTest, TextureMaxSize)
{
    testTextureSizeError();
}

// Test that drawing works correctly RGBA 3D texture
TEST_P(Texture3DTestES2, RGBA)
{
    ANGLE_SKIP_TEST_IF(!hasTexture3DExt());

    // http://anglebug.com/5728
    ANGLE_SKIP_TEST_IF(IsOzone());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, mTexture3D);
    std::vector<GLColor> texDataGreen(2u * 2u * 2u, GLColor::green);
    std::vector<GLColor> texDataRed(1u * 1u * 1u, GLColor::red);
    glTexImage3DOES(GL_TEXTURE_3D, 0, GL_RGBA, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    texDataGreen.data());
    glTexImage3DOES(GL_TEXTURE_3D, 1, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    texDataRed.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that drawing works correctly Luminance 3D texture
TEST_P(Texture3DTestES2, Luminance)
{
    ANGLE_SKIP_TEST_IF(!hasTexture3DExt());

    // http://anglebug.com/5728
    ANGLE_SKIP_TEST_IF(IsOzone());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, mTexture3D);
    std::vector<GLubyte> texData(2u * 2u * 2u, 125);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage3DOES(GL_TEXTURE_3D, 0, GL_LUMINANCE, 2, 2, 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                    texData.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(125, 125, 125, 255));
}

// Test that drawing works correctly with glCopyTexSubImage3D
TEST_P(Texture3DTestES2, CopySubImageRGBA)
{
    ANGLE_SKIP_TEST_IF(!hasTexture3DExt());

    // http://anglebug.com/5728
    ANGLE_SKIP_TEST_IF(IsOzone());

    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, mTexture3D);
    std::vector<GLColor> texDataRed(4u * 4u * 4u, GLColor::red);
    glTexImage3DOES(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    texDataRed.data());
    glTexImage3DOES(GL_TEXTURE_3D, 1, GL_RGBA, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    texDataRed.data());
    glTexImage3DOES(GL_TEXTURE_3D, 2, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    texDataRed.data());
    glCopyTexSubImage3DOES(GL_TEXTURE_3D, 1, 0, 0, 0, 0, 0, 2, 2);
    glCopyTexSubImage3DOES(GL_TEXTURE_3D, 1, 0, 0, 1, 0, 0, 2, 2);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);

    EXPECT_GL_NO_ERROR();

    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    glUseProgram(mProgram);
    glUniform1f(glGetUniformLocation(mProgram, "level"), 1);
    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
}

TEST_P(Texture3DTestES2, CopySubImageLuminance)
{
    ANGLE_SKIP_TEST_IF(!hasTexture3DExt());

    // http://anglebug.com/5728
    ANGLE_SKIP_TEST_IF(IsOzone());

    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, mTexture3D);
    glTexImage3DOES(GL_TEXTURE_3D, 0, GL_LUMINANCE, 4, 4, 4, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                    nullptr);
    glTexImage3DOES(GL_TEXTURE_3D, 1, GL_LUMINANCE, 2, 2, 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                    nullptr);
    glTexImage3DOES(GL_TEXTURE_3D, 2, GL_LUMINANCE, 1, 1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                    nullptr);
    glCopyTexSubImage3DOES(GL_TEXTURE_3D, 1, 0, 0, 0, 0, 0, 2, 2);
    glCopyTexSubImage3DOES(GL_TEXTURE_3D, 1, 0, 0, 1, 0, 0, 2, 2);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);

    EXPECT_GL_NO_ERROR();

    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    glUseProgram(mProgram);
    glUniform1f(glGetUniformLocation(mProgram, "level"), 1);
    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);
}

TEST_P(Texture3DTestES2, CopySubImageAlpha)
{
    ANGLE_SKIP_TEST_IF(!hasTexture3DExt());

    // http://anglebug.com/5728
    ANGLE_SKIP_TEST_IF(IsOzone());

    glClearColor(1, 0, 0, 0.5);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, mTexture3D);
    glTexImage3DOES(GL_TEXTURE_3D, 0, GL_ALPHA, 4, 4, 4, 0, GL_ALPHA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage3DOES(GL_TEXTURE_3D, 1, GL_ALPHA, 2, 2, 2, 0, GL_ALPHA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage3DOES(GL_TEXTURE_3D, 2, GL_ALPHA, 1, 1, 1, 0, GL_ALPHA, GL_UNSIGNED_BYTE, nullptr);
    glCopyTexSubImage3DOES(GL_TEXTURE_3D, 1, 0, 0, 0, 0, 0, 2, 2);
    glCopyTexSubImage3DOES(GL_TEXTURE_3D, 1, 0, 0, 1, 0, 0, 2, 2);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);

    EXPECT_GL_NO_ERROR();

    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    glUseProgram(mProgram);
    glUniform1f(glGetUniformLocation(mProgram, "level"), 1);
    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(0, 0, 0, 128), 1.0);
}

// Verify shrinking a texture with glTexStorage2D works correctly
TEST_P(Texture2DTestES3, ChangeTexSizeWithTexStorage)
{
    // TODO: http://anglebug.com/5256
    ANGLE_SKIP_TEST_IF(IsWindows() && IsOpenGL());

    constexpr uint32_t kSizeLarge = 128;
    constexpr uint32_t kSizeSmall = 64;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // Create the texture with 'large' dimensions
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kSizeLarge, kSizeLarge, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    ASSERT_GL_NO_ERROR();

    GLFramebuffer destFbo;
    glBindFramebuffer(GL_FRAMEBUFFER, destFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture2D, 0);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Draw with the new texture so it's created in the back end
    ANGLE_GL_PROGRAM(blueProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Blue());
    glUseProgram(blueProgram);
    drawQuad(blueProgram.get(), std::string(essl1_shaders::PositionAttrib()), 0.0f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_RECT_EQ(0, 0, kSizeLarge, kSizeLarge, GLColor::blue);

    // Shrink the texture
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kSizeSmall, kSizeSmall);
    ASSERT_GL_NO_ERROR();

    // Create a source texture/FBO to blit from
    GLTexture sourceTex;
    glBindTexture(GL_TEXTURE_2D, sourceTex.get());
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kSizeSmall, kSizeSmall);
    ASSERT_GL_NO_ERROR();
    GLFramebuffer sourceFbo;
    glBindFramebuffer(GL_FRAMEBUFFER, sourceFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sourceTex, 0);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    // Fill the source texture with green
    ANGLE_GL_PROGRAM(greenProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
    glUseProgram(greenProgram);
    drawQuad(greenProgram.get(), std::string(essl1_shaders::PositionAttrib()), 0.0f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_RECT_EQ(0, 0, kSizeSmall, kSizeSmall, GLColor::green);

    // Blit the source (green) to the destination
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFbo);
    glBlitFramebuffer(0, 0, kSizeSmall, kSizeSmall, 0, 0, kSizeSmall, kSizeSmall,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Render to the default framebuffer sampling from the blited texture and verify it's green
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    ANGLE_GL_PROGRAM(texProgram, essl1_shaders::vs::Texture2D(), essl1_shaders::fs::Texture2D());
    glUseProgram(texProgram);
    drawQuad(texProgram.get(), std::string(essl1_shaders::PositionAttrib()), 0.0f);
    EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth(), getWindowHeight(), GLColor::green);
}

// Regression test for http://crbug.com/949985 to make sure dirty bits are propagated up from
// TextureImpl and the texture is synced before being used in a draw call.
TEST_P(Texture2DTestES3, TextureImplPropogatesDirtyBits)
{
    ANGLE_SKIP_TEST_IF(IsIntel() && IsOpenGL());
    // Flaky hangs on Win10 AMD RX 550 GL. http://anglebug.com/3371
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && IsOpenGL());
    // D3D Debug device reports an error. http://anglebug.com/3501
    ANGLE_SKIP_TEST_IF(IsWindows() && IsD3D11());
    // Support copy from levels outside the image range. http://anglebug.com/4733
    ANGLE_SKIP_TEST_IF(IsVulkan());

    // The workaround in the GL backend required to trigger this bug generates driver warning
    // messages.
    ScopedIgnorePlatformMessages ignoreMessages;

    setUpProgram();
    glUseProgram(mProgram);
    glActiveTexture(GL_TEXTURE0 + mTexture2DUniformLocation);

    GLTexture dest;
    glBindTexture(GL_TEXTURE_2D, dest);

    GLTexture source;
    glBindTexture(GL_TEXTURE_2D, source);

    // Put data in mip 0 and 1
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 GLColor::red.data());
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 GLColor::green.data());

    // Disable mipmapping so source is complete
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Force the dirty bits to be synchronized in source
    drawQuad(mProgram, "position", 1.0f);

    // Copy from mip 1 of the source.  In the GL backend this internally sets the base level to mip
    // 1 and sets a dirty bit.
    glCopyTextureCHROMIUM(source, 1, GL_TEXTURE_2D, dest, 0, GL_RGBA, GL_UNSIGNED_BYTE, GL_FALSE,
                          GL_FALSE, GL_FALSE);

    // Draw again, assertions are generated if the texture has internal dirty bits at draw time
    drawQuad(mProgram, "position", 1.0f);
}

// This test case changes the base level of a texture that's attached to a framebuffer, clears every
// level to green, and then samples the texture when rendering. Test is taken from
// https://www.khronos.org/registry/webgl/sdk/tests/conformance2/rendering/framebuffer-texture-changing-base-level.html
TEST_P(Texture2DTestES3, FramebufferTextureChangingBaselevel)
{
    // TODO(cnorthrop): Failing on Vulkan/Windows/AMD. http://anglebug.com/3996
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsWindows() && IsAMD());

    setUpProgram();

    constexpr GLint width  = 8;
    constexpr GLint height = 4;

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create all mipmap levels for the texture from level 0 to the 1x1 pixel level.
    GLint level  = 0;
    GLint levelW = width;
    GLint levelH = height;
    glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, levelW, levelH, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    while (levelW > 1 || levelH > 1)
    {
        ++level;
        levelW = static_cast<GLint>(std::max(1.0, std::floor(width / std::pow(2, level))));
        levelH = static_cast<GLint>(std::max(1.0, std::floor(height / std::pow(2, level))));
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, levelW, levelH, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
    }

    // Clear each level of the texture using an FBO. Change the base level to match the level used
    // for the FBO on each iteration.
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    level  = 0;
    levelW = width;
    levelH = height;
    while (levelW > 1 || levelH > 1)
    {
        levelW = static_cast<GLint>(std::floor(width / std::pow(2, level)));
        levelH = static_cast<GLint>(std::floor(height / std::pow(2, level)));

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, level);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, level);

        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        EXPECT_GL_NO_ERROR();

        glClearColor(0, 1, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

        ++level;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, 16, 16);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that changing the base level of a texture after redefining a level outside the mip-chain
// preserves the other mips' data.
TEST_P(Texture2DBaseMaxTestES3, ExtendMipChainAfterRedefine)
{
    // http://anglebug.com/4699
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsIntel() && IsOSX());

    // http://anglebug.com/5153
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsNVIDIA() && IsOSX());

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    std::array<GLColor, getTotalMipDataSize(kMip0Size)> mipData;
    fillMipData(mipData.data(), kMip0Size, kMipColors);

    for (size_t mip = 1; mip < kMipCount; ++mip)
    {
        glTexImage2D(GL_TEXTURE_2D, mip, GL_RGBA8, kMip0Size >> mip, kMip0Size >> mip, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, mipData.data() + getMipDataOffset(kMip0Size, mip));
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 1);
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Mip 1 is green.  Verify this.
    EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[1]);

    // http://anglebug.com/4709
    ANGLE_SKIP_TEST_IF(IsOpenGL() && (IsIntel() || IsAMD()) && IsWindows());

    // Add mip 0 and rebase the mip chain.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kMip0Size, kMip0Size, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 mipData.data() + getMipDataOffset(kMip0Size, 0));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);

    // Mip 1 should still be green.
    EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[1]);

    // Verify the other mips too.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 2);
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[2]);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 3);
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[3]);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // http://anglebug.com/4704
    ANGLE_SKIP_TEST_IF(IsVulkan());

    EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[0]);
}

// Test that changing the base level of a texture multiple times preserves the data.
TEST_P(Texture2DBaseMaxTestES3, PingPongBaseLevel)
{
    testPingPongBaseLevel(false);
}
TEST_P(Texture2DBaseMaxTestES3, PingPongBaseLevelImmutable)
{
    testPingPongBaseLevel(true);
}
void Texture2DBaseMaxTestES3::testPingPongBaseLevel(bool immutable)
{
    // http://anglebug.com/4710
    ANGLE_SKIP_TEST_IF(IsD3D());

    // http://anglebug.com/4711
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsAMD() && IsWindows());

    // http://anglebug.com/4701
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsIntel() && IsOSX());

    initTest(immutable);

    // Ping pong a few times.
    for (uint32_t tries = 0; tries < 2; ++tries)
    {
        // Rebase to different mips and verify mips.
        for (uint32_t base = 0; base < kMipCount; ++base)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, base);
            for (uint32_t lod = 0; lod < kMipCount - base; ++lod)
            {
                setLodUniform(lod);
                drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
                EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[base + lod]);
            }
        }

        // Rebase backwards and verify mips.
        for (uint32_t base = kMipCount - 2; base > 0; --base)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, base);
            for (uint32_t lod = 0; lod < kMipCount - base; ++lod)
            {
                setLodUniform(lod);
                drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
                EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[base + lod]);
            }
        }
    }
}

// Test that glTexSubImage2D after incompatibly redefining a mip level correctly applies the update
// after the redefine data.
TEST_P(Texture2DBaseMaxTestES3, SubImageAfterRedefine)
{
    initTest(false);

    // Test that all mips have the expected data initially (this makes sure the texture image is
    // created already).
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[lod]);
    }

    // Redefine every level, followed by a glTexSubImage2D
    const GLColor kNewMipColors[kMipCount] = {
        GLColor::yellow,
        GLColor::cyan,
        GLColor(127, 0, 0, 255),
        GLColor(0, 127, 0, 255),
    };
    std::array<GLColor, getTotalMipDataSize(kMip0Size * 2)> newMipData;
    fillMipData(newMipData.data(), kMip0Size * 2, kNewMipColors);

    const GLColor kSubImageMipColors[kMipCount] = {
        GLColor(0, 0, 127, 255),
        GLColor(127, 127, 0, 255),
        GLColor(0, 127, 127, 255),
        GLColor(127, 0, 127, 255),
    };
    std::array<GLColor, getTotalMipDataSize(kMip0Size)> subImageMipData;
    fillMipData(subImageMipData.data(), kMip0Size, kSubImageMipColors);

    for (size_t mip = 0; mip < kMipCount; ++mip)
    {
        // Redefine the level.
        size_t newMipSize = (kMip0Size * 2) >> mip;
        glTexImage2D(GL_TEXTURE_2D, mip, GL_RGBA8, newMipSize, newMipSize, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, newMipData.data() + getMipDataOffset(kMip0Size * 2, mip));

        // Immediately follow that with a subimage update.
        glTexSubImage2D(GL_TEXTURE_2D, mip, 0, 0, kMip0Size >> mip, kMip0Size >> mip, GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        subImageMipData.data() + getMipDataOffset(kMip0Size, mip));
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, kMipCount - 1);

    // Test that the texture looks as expected.
    const int w = getWindowWidth() - 1;
    const int h = getWindowHeight() - 1;
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kSubImageMipColors[lod]);
        EXPECT_PIXEL_COLOR_EQ(w, 0, kNewMipColors[lod]);
        EXPECT_PIXEL_COLOR_EQ(0, h, kNewMipColors[lod]);
        EXPECT_PIXEL_COLOR_EQ(w, h, kNewMipColors[lod]);
    }
}

// Test that incompatibly redefining a level then redefining it back to its original size works.
TEST_P(Texture2DBaseMaxTestES3, IncompatiblyRedefineLevelThenRevert)
{
    initTest(false);

    // Test that all mips have the expected data initially (this makes sure the texture image is
    // created already).
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[lod]);
    }

    // Redefine Mip 1 to be larger.
    constexpr size_t kLargeMip1Size = getMipDataSize(kMip0Size * 2, 1);
    std::array<GLColor, kLargeMip1Size> interimMipData;
    std::fill(interimMipData.data(), interimMipData.data() + kLargeMip1Size, GLColor::yellow);

    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kMip0Size, kMip0Size, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 interimMipData.data());

    // Redefine Mip 1 back to its original size.
    constexpr size_t kNormalMip1Size = getMipDataSize(kMip0Size, 1);
    std::array<GLColor, kLargeMip1Size> newMipData;
    std::fill(newMipData.data(), newMipData.data() + kNormalMip1Size, GLColor::cyan);

    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kMip0Size / 2, kMip0Size / 2, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, newMipData.data());

    // Verify texture colors.
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, lod == 1 ? GLColor::cyan : kMipColors[lod]);
    }
}

// Test that redefining every level of a texture to another format works.  The format uses more
// bits per component, to ensure alignment requirements for the new format are taken into account.
TEST_P(Texture2DBaseMaxTestES3, RedefineEveryLevelToAnotherFormat)
{
    initTest(false);

    // Test that all mips have the expected data initially (this makes sure the texture image is
    // created already).
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[lod]);
    }

    const GLColor32F kNewMipColors[kMipCount] = {
        GLColor32F(1.0, 1.0, 0.0, 1.0f),
        GLColor32F(1.0, 0.0, 1.0, 1.0f),
        GLColor32F(0.0, 1.0, 1.0, 1.0f),
        GLColor32F(1.0, 1.0, 1.0, 1.0f),
    };

    std::array<GLColor32F, getTotalMipDataSize(kMip0Size)> newMipData;
    fillMipData(newMipData.data(), kMip0Size, kNewMipColors);

    // Redefine every level with the new format.
    for (size_t mip = 0; mip < kMipCount; ++mip)
    {
        glTexImage2D(GL_TEXTURE_2D, mip, GL_RGBA32F, kMip0Size >> mip, kMip0Size >> mip, 0, GL_RGBA,
                     GL_FLOAT, newMipData.data() + getMipDataOffset(kMip0Size, mip));
    }

    // Verify texture colors.
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);

        GLColor32F mipColor32F = kNewMipColors[lod];
        GLColor mipColor(static_cast<GLubyte>(std::roundf(mipColor32F.R * 255)),
                         static_cast<GLubyte>(std::roundf(mipColor32F.G * 255)),
                         static_cast<GLubyte>(std::roundf(mipColor32F.B * 255)),
                         static_cast<GLubyte>(std::roundf(mipColor32F.A * 255)));

        EXPECT_PIXEL_COLOR_EQ(0, 0, mipColor);
    }
}

// Test that generating mipmaps after change base level.
TEST_P(Texture2DBaseMaxTestES3, GenerateMipmapAfterRebase)
{
    // http://anglebug.com/5880
    ANGLE_SKIP_TEST_IF(IsOSX() && IsARM64() && IsDesktopOpenGL());

    testGenerateMipmapAfterRebase(false);
}

TEST_P(Texture2DBaseMaxTestES3, GenerateMipmapAfterRebaseImmutable)
{
    // http://anglebug.com/4710
    ANGLE_SKIP_TEST_IF(IsD3D());
    // http://anglebug.com/5798
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsNVIDIA());
    // http://anglebug.com/5880
    ANGLE_SKIP_TEST_IF(IsOSX() && IsARM64() && IsDesktopOpenGL());

    testGenerateMipmapAfterRebase(true);
}

void Texture2DBaseMaxTestES3::testGenerateMipmapAfterRebase(bool immutable)
{
    initTest(immutable);

    // Test that all mips have the expected data initially (this makes sure the texture image is
    // created already).
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[lod]);
    }

    // Update level 1 (any level would do other than 0) with new data
    const GLColor kNewMipColor = GLColor::yellow;
    std::array<GLColor, getMipDataSize(kMip0Size >> 1, 0)> newMipData;
    std::fill(newMipData.begin(), newMipData.end(), kNewMipColor);

    glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, kMip0Size >> 1, kMip0Size >> 1, GL_RGBA,
                    GL_UNSIGNED_BYTE, newMipData.data());

    // Change base level and max level and then generate mipmaps. This should redefine level 1 and 2
    // with kNewMipColor and leave levels 0 and 3 unchanged.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, kMipCount - 2);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, kMipCount - 1);

    // Test that the texture looks as expected.
    const int w = getWindowWidth() - 1;
    const int h = getWindowHeight() - 1;
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        if (lod == 0)
        {
            EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[lod]) << "lod " << lod;
            EXPECT_PIXEL_COLOR_EQ(w, 0, kMipColors[lod]) << "lod " << lod;
            EXPECT_PIXEL_COLOR_EQ(0, h, kMipColors[lod]) << "lod " << lod;
            EXPECT_PIXEL_COLOR_EQ(w, h, kMipColors[lod]) << "lod " << lod;
        }
        else if (lod == kMipCount - 1)
        {
            EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[lod]) << "lod " << lod;
            EXPECT_PIXEL_COLOR_EQ(w, 0, kMipColors[lod]) << "lod " << lod;
            EXPECT_PIXEL_COLOR_EQ(0, h, kMipColors[lod]) << "lod " << lod;
            EXPECT_PIXEL_COLOR_EQ(w, h, kMipColors[lod]) << "lod " << lod;
        }
        else
        {
            EXPECT_PIXEL_COLOR_EQ(0, 0, kNewMipColor) << "lod " << lod;
            EXPECT_PIXEL_COLOR_EQ(w, 0, kNewMipColor) << "lod " << lod;
            EXPECT_PIXEL_COLOR_EQ(0, h, kNewMipColor) << "lod " << lod;
            EXPECT_PIXEL_COLOR_EQ(w, h, kNewMipColor) << "lod " << lod;
        }
    }
}

// Test that generating mipmaps after incompatibly redefining a level works.
TEST_P(Texture2DBaseMaxTestES3, GenerateMipmapAfterRedefine)
{
    initTest(false);

    // Test that all mips have the expected data initially (this makes sure the texture image is
    // created already).
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[lod]);
    }

    // Redefine level 1 (any level would do other than 0) to an incompatible size, say the same size
    // as level 0.
    const GLColor kNewMipColor = GLColor::yellow;
    std::array<GLColor, getMipDataSize(kMip0Size, 0)> newMipData;
    std::fill(newMipData.begin(), newMipData.end(), kNewMipColor);

    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kMip0Size, kMip0Size, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 newMipData.data());

    // Generate mipmaps.  This should redefine level 1 back to being compatible with level 0.
    glGenerateMipmap(GL_TEXTURE_2D);

    // Test that the texture looks as expected.
    const int w = getWindowWidth() - 1;
    const int h = getWindowHeight() - 1;
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[0]);
        EXPECT_PIXEL_COLOR_EQ(w, 0, kMipColors[0]);
        EXPECT_PIXEL_COLOR_EQ(0, h, kMipColors[0]);
        EXPECT_PIXEL_COLOR_EQ(w, h, kMipColors[0]);
    }
}

// Test that generating mipmaps after incompatibly redefining a level while simultaneously changing
// the base level works.
TEST_P(Texture2DBaseMaxTestES3, GenerateMipmapAfterRedefineAndRebase)
{
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && IsOpenGL());

    // http://crbug.com/1100613
    ANGLE_SKIP_TEST_IF(IsNVIDIAShield());

    // TODO(anglebug.com/5360): Failing on ARM-based Apple DTKs.
    ANGLE_SKIP_TEST_IF(IsOSX() && IsARM64() && IsDesktopOpenGL());

    initTest(false);

    // Test that all mips have the expected data initially (this makes sure the texture image is
    // created already).
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[lod]) << lod;
    }

    // Redefine level 2 to an incompatible size, say the same size as level 0.
    const GLColor kNewMipColor = GLColor::yellow;
    std::array<GLColor, getMipDataSize(kMip0Size, 0)> newMipData;
    std::fill(newMipData.begin(), newMipData.end(), kNewMipColor);

    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, kMip0Size, kMip0Size, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 newMipData.data());

    // Set base level of the texture to 1 then generate mipmaps.  Level 2 that's redefined should
    // go back to being compatibly defined.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Test that the texture looks as expected.
    const int w = getWindowWidth() - 1;
    const int h = getWindowHeight() - 1;
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[1]) << lod;
        EXPECT_PIXEL_COLOR_EQ(w, 0, kMipColors[1]) << lod;
        EXPECT_PIXEL_COLOR_EQ(0, h, kMipColors[1]) << lod;
        EXPECT_PIXEL_COLOR_EQ(w, h, kMipColors[1]) << lod;
    }

    // Redefine level 1 (current base level) to an incompatible size.
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kMip0Size, kMip0Size, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 newMipData.data());

    // Set base level of the texture back to 0 then generate mipmaps.  Level 1 should go back to
    // being compatibly defined.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Test that the texture looks as expected.
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[0]) << lod;
        EXPECT_PIXEL_COLOR_EQ(w, 0, kMipColors[0]) << lod;
        EXPECT_PIXEL_COLOR_EQ(0, h, kMipColors[0]) << lod;
        EXPECT_PIXEL_COLOR_EQ(w, h, kMipColors[0]) << lod;
    }
}

// Test that generating mipmaps after incompatibly redefining the base level of the texture works.
TEST_P(Texture2DBaseMaxTestES3, GenerateMipmapAfterRedefiningBase)
{
    initTest(false);

    // Test that all mips have the expected data initially (this makes sure the texture image is
    // created already).
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[lod]);
    }

    // Redefine level 0 to an incompatible size.
    const GLColor kNewMipColor = GLColor::yellow;
    std::array<GLColor, getMipDataSize(kMip0Size * 2, 0)> newMipData;
    std::fill(newMipData.begin(), newMipData.end(), kNewMipColor);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kMip0Size * 2, kMip0Size * 2, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, newMipData.data());

    // Generate mipmaps.
    glGenerateMipmap(GL_TEXTURE_2D);

    // Test that the texture looks as expected.
    const int w = getWindowWidth() - 1;
    const int h = getWindowHeight() - 1;
    for (uint32_t lod = 0; lod < kMipCount + 1; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kNewMipColor);
        EXPECT_PIXEL_COLOR_EQ(w, 0, kNewMipColor);
        EXPECT_PIXEL_COLOR_EQ(0, h, kNewMipColor);
        EXPECT_PIXEL_COLOR_EQ(w, h, kNewMipColor);
    }
}

// Test that generating mipmaps after incompatibly redefining the base level while simultaneously
// changing MAX_LEVEL works.
TEST_P(Texture2DBaseMaxTestES3, GenerateMipmapAfterRedefiningBaseAndChangingMax)
{
    initTest(false);

    // Test that all mips have the expected data initially (this makes sure the texture image is
    // created already).
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[lod]);
    }

    // Redefine level 0 to an incompatible size.
    const GLColor kNewMipColor = GLColor::yellow;
    std::array<GLColor, getMipDataSize(kMip0Size * 2, 0)> newMipData;
    std::fill(newMipData.begin(), newMipData.end(), kNewMipColor);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kMip0Size * 2, kMip0Size * 2, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, newMipData.data());

    // Set max level of the texture to 2 then generate mipmaps.
    constexpr uint32_t kMaxLevel = 2;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, kMaxLevel);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Test that the texture looks as expected.
    const int w = getWindowWidth() - 1;
    const int h = getWindowHeight() - 1;
    for (uint32_t lod = 0; lod <= kMaxLevel; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kNewMipColor);
        EXPECT_PIXEL_COLOR_EQ(w, 0, kNewMipColor);
        EXPECT_PIXEL_COLOR_EQ(0, h, kNewMipColor);
        EXPECT_PIXEL_COLOR_EQ(w, h, kNewMipColor);
    }
}

// Test that stage invalid texture levels work.
TEST_P(Texture2DBaseMaxTestES3, StageInvalidLevels)
{
    constexpr uint32_t kMaxLevel           = 2;
    const GLColor kMipColor[kMaxLevel + 1] = {GLColor::red, GLColor::green, GLColor::blue};

    initTest(false);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    std::vector<GLColor> texDataCyan(2u * 2u, GLColor::cyan);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, texDataCyan.data());
    setLodUniform(0);
    drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_NO_ERROR();

    std::vector<GLColor> texDataGreen(2u * 2u, GLColor::green);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &GLColor::blue);
    drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_NO_ERROR();

    std::vector<GLColor> texDataRed(4u * 4u, GLColor::red);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, texDataRed.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);

    // Test that the texture looks as expected.
    const int w = getWindowWidth() - 1;
    const int h = getWindowHeight() - 1;
    for (uint32_t lod = 0; lod <= kMaxLevel; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColor[lod]);
        EXPECT_PIXEL_COLOR_EQ(w, 0, kMipColor[lod]);
        EXPECT_PIXEL_COLOR_EQ(0, h, kMipColor[lod]);
        EXPECT_PIXEL_COLOR_EQ(w, h, kMipColor[lod]);
    }
}

// Test redefine a mutable texture into an immutable texture.
TEST_P(Texture2DBaseMaxTestES3, RedefineMutableToImmutable)
{
    // http://anglebug.com/4710
    ANGLE_SKIP_TEST_IF(IsD3D());

    // http://anglebug.com/4701
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsIntel() && IsOSX());

    constexpr uint32_t kBaseLevel          = 1;
    const GLColor kNewMipColors[kMipCount] = {
        GLColor::yellow,
        GLColor::cyan,
        GLColor::white,
        GLColor(127u, 127u, 127u, 255u),
    };

    initTest(false);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, kBaseLevel);

    // Test that all mips have the expected data
    for (uint32_t lod = kBaseLevel; lod < kMipCount; ++lod)
    {
        setLodUniform(lod - kBaseLevel);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[lod]);
    }

    glTexStorage2D(GL_TEXTURE_2D, kMipCount, GL_RGBA8, kMip0Size, kMip0Size);
    std::array<GLColor, getTotalMipDataSize(kMip0Size)> mipData;
    fillMipData(mipData.data(), kMip0Size, kNewMipColors);
    for (size_t mip = 0; mip < kMipCount; ++mip)
    {
        glTexSubImage2D(GL_TEXTURE_2D, mip, 0, 0, kMip0Size >> mip, kMip0Size >> mip, GL_RGBA,
                        GL_UNSIGNED_BYTE, mipData.data() + getMipDataOffset(kMip0Size, mip));
    }

    // Test that all enabled mips have the expected data
    for (uint32_t lod = kBaseLevel; lod < kMipCount; ++lod)
    {
        setLodUniform(lod - kBaseLevel);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kNewMipColors[lod]);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    for (uint32_t lod = 0; lod < kBaseLevel; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kNewMipColors[lod]);
    }
}

// Test that redefine a level with incompatible size beyond the max level.
TEST_P(Texture2DBaseMaxTestES3, RedefineIncompatibleLevelBeyondMaxLevel)
{
    initTest(false);

    // Test that all mips have the expected data initially (this makes sure the texture image is
    // created already).
    for (uint32_t lod = 0; lod < kMipCount; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[lod]);
    }

    uint32_t maxLevel = 1;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, maxLevel);

    // Update level 0
    const GLColor kNewMipLevle0Color = GLColor::yellow;
    std::array<GLColor, getMipDataSize(kMip0Size, 0)> newMipData;
    std::fill(newMipData.begin(), newMipData.end(), kNewMipLevle0Color);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kMip0Size, kMip0Size, GL_RGBA, GL_UNSIGNED_BYTE,
                    newMipData.data());

    // Update level 2 with incompatible data
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, 10, 10, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 newMipData.data());
    EXPECT_GL_NO_ERROR();

    // Test that the texture looks as expected.
    const int w = getWindowWidth() - 1;
    const int h = getWindowHeight() - 1;
    for (uint32_t lod = 0; lod < maxLevel; ++lod)
    {
        setLodUniform(lod);
        drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
        if (lod == 0)
        {
            EXPECT_PIXEL_COLOR_EQ(0, 0, kNewMipLevle0Color);
            EXPECT_PIXEL_COLOR_EQ(w, 0, kNewMipLevle0Color);
            EXPECT_PIXEL_COLOR_EQ(0, h, kNewMipLevle0Color);
            EXPECT_PIXEL_COLOR_EQ(w, h, kNewMipLevle0Color);
        }
        else
        {
            EXPECT_PIXEL_COLOR_EQ(0, 0, kMipColors[lod]);
            EXPECT_PIXEL_COLOR_EQ(w, 0, kMipColors[lod]);
            EXPECT_PIXEL_COLOR_EQ(0, h, kMipColors[lod]);
            EXPECT_PIXEL_COLOR_EQ(w, h, kMipColors[lod]);
        }
    }
}

// Port test from web_gl/conformance2/textures/misc/fuzz-545-immutable-tex-render-feedback.html.
// What this tries to do is create a render feedback loop and ensure it is not crashing.
TEST_P(Texture2DBaseMaxTestES3, Fuzz545ImmutableTexRenderFeedback)
{
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Texture2D(), essl1_shaders::fs::Texture2D());

    constexpr uint32_t MIPS = 2;
    constexpr uint32_t SIZE = 10;

    GLTexture immutTex;
    glBindTexture(GL_TEXTURE_2D, immutTex);
    glTexStorage2D(GL_TEXTURE_2D, MIPS, GL_RGBA8, SIZE, SIZE);

    GLTexture mutTex;
    glBindTexture(GL_TEXTURE_2D, mutTex);
    for (uint32_t mip = 0; mip < MIPS; mip++)
    {
        const uint32_t size = SIZE >> mip;
        glTexImage2D(GL_TEXTURE_2D, mip, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
    }

    constexpr GLenum MAG_FILTERS[] = {GL_LINEAR, GL_NEAREST};
    constexpr GLenum MIN_FILTERS[] = {
        GL_LINEAR,  GL_LINEAR_MIPMAP_LINEAR,  GL_LINEAR_MIPMAP_NEAREST,
        GL_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST_MIPMAP_NEAREST};

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    const GLuint texs[] = {immutTex, mutTex};
    for (const GLuint tex : texs)
    {
        glBindTexture(GL_TEXTURE_2D, tex);

        for (GLuint level_prime_base = 0; level_prime_base < (MIPS + 1); level_prime_base++)
        {  // `level_base` in GLES
            // ES 3.0.6 p150
            GLuint _level_base = level_prime_base;
            if (tex == immutTex)
            {
                _level_base = std::min(_level_base, MIPS - 1);
            }
            const GLuint level_base = _level_base;

            for (GLuint _level_prime_max = (level_prime_base - 1); _level_prime_max < (MIPS + 2);
                 _level_prime_max++)
            {  // `q` in GLES
                if (_level_prime_max < 0)
                    continue;
                if (_level_prime_max == (MIPS + 1))
                {
                    _level_prime_max = 10000;  // This is the default, after all!
                }
                const GLuint level_prime_max = _level_prime_max;

                // ES 3.0.6 p150
                GLuint _level_max = level_prime_max;
                if (tex == immutTex)
                {
                    _level_max = std::min(std::max(level_base, level_prime_max), MIPS - 1);
                }
                const GLuint level_max = _level_max;

                const GLuint p = std::floor((float)std::log2(SIZE)) + level_base;
                const GLuint q = std::min(p, level_max);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, level_prime_base);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, level_prime_max);

                const bool mipComplete = (q <= MIPS - 1);

                for (const GLenum minFilter : MIN_FILTERS)
                {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);

                    for (const GLenum magFilter : MAG_FILTERS)
                    {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

                        for (GLuint dstMip = 0; dstMip < (MIPS + 1); dstMip++)
                        {
                            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                   GL_TEXTURE_2D, tex, dstMip);

                            // ES3.0 p213-214
                            bool fbComplete = true;

                            // * "The width and height of `image` are non-zero"
                            fbComplete &= (0 <= dstMip && dstMip <= MIPS - 1);

                            if (tex != immutTex)
                            {  // "...does not name an immutable-format texture..."
                                // * "...the value of [level] must be in the range `[level_base,
                                // q]`"
                                fbComplete &= (level_base <= dstMip && dstMip <= q);

                                // * "...the value of [level] is not `level_base`, then the texture
                                // must be mipmap complete"
                                if (dstMip != level_base)
                                {
                                    fbComplete &= mipComplete;
                                }
                            }

                            // -
                            GLenum expectError  = 0;
                            GLenum expectStatus = GL_FRAMEBUFFER_COMPLETE;
                            if (!fbComplete)
                            {
                                expectStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                                expectError  = GL_INVALID_FRAMEBUFFER_OPERATION;
                            }

                            // -
                            EXPECT_GLENUM_EQ(expectStatus,
                                             glCheckFramebufferStatus(GL_FRAMEBUFFER));

                            drawQuad(program, essl1_shaders::PositionAttrib(), 0.5, 1.0f, true);
                            EXPECT_EQ(expectError, glGetError());
                        }
                    }
                }
            }
        }
    }
}

// Test to check that texture completeness is determined correctly when the texture base level is
// greater than 0, and also that level 0 is not sampled when base level is greater than 0.
TEST_P(Texture2DTestES3, DrawWithBaseLevel1)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    std::vector<GLColor> texDataRed(4u * 4u, GLColor::red);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, texDataRed.data());
    std::vector<GLColor> texDataGreen(2u * 2u, GLColor::green);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test basic GL_EXT_copy_image copy without any bound textures
TEST_P(Texture2DTestES3, CopyImage)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_copy_image"));

    std::vector<GLColor> texDataRed(4u * 4u, GLColor::red);
    GLTexture srcTexture;
    GLTexture destTexture;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, destTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, texDataRed.data());

    std::vector<GLColor> texDataGreen(4u * 4u, GLColor::green);
    glBindTexture(GL_TEXTURE_2D, srcTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());

    glBindTexture(GL_TEXTURE_2D, 0);

    // copy
    glCopyImageSubDataEXT(srcTexture, GL_TEXTURE_2D, 0, 2, 2, 0, destTexture, GL_TEXTURE_2D, 0, 2,
                          2, 0, 2, 2, 1);

    glBindTexture(GL_TEXTURE_2D, destTexture);

    EXPECT_GL_NO_ERROR();

    glViewport(0, 0, 4, 4);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_RECT_EQ(2, 2, 2, 2, GLColor::green);
    EXPECT_PIXEL_RECT_EQ(0, 0, 4, 2, GLColor::red);
    EXPECT_PIXEL_RECT_EQ(0, 0, 2, 4, GLColor::red);
}

// Test GL_EXT_copy_image compressed texture copy with mipmaps smaller than the block size
TEST_P(Texture2DTestES3, CopyCompressedImageMipMaps)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_copy_image"));
    // TODO(http://anglebug.com/5634): Fix calls to vkCmdCopyBufferToImage() with images smaller
    // than the compressed format block size.
    ANGLE_SKIP_TEST_IF(getEGLWindow()->isFeatureEnabled(Feature::AllocateNonZeroMemory));

    constexpr uint32_t kSize             = 4;
    constexpr size_t kNumLevels          = 3;
    const uint8_t CompressedImageETC1[8] = {0x0, 0x0, 0xf8, 0x2, 0xff, 0xff, 0x0, 0x0};

    GLTexture srcTexture;
    glBindTexture(GL_TEXTURE_2D, srcTexture);
    for (size_t level = 0; level < kNumLevels; ++level)
    {
        glCompressedTexImage2D(GL_TEXTURE_2D, level, GL_ETC1_RGB8_OES, kSize >> level,
                               kSize >> level, 0, 8, CompressedImageETC1);
        EXPECT_GL_NO_ERROR();
    }

    GLTexture destTexture;
    glBindTexture(GL_TEXTURE_2D, destTexture);
    for (size_t level = 0; level < kNumLevels; ++level)
    {
        glCompressedTexImage2D(GL_TEXTURE_2D, level, GL_ETC1_RGB8_OES, kSize >> level,
                               kSize >> level, 0, 8, nullptr);
        EXPECT_GL_NO_ERROR();
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    // copy
    for (size_t level = 0; level < kNumLevels; ++level)
    {
        glCopyImageSubDataEXT(srcTexture, GL_TEXTURE_2D, level, 0, 0, 0, destTexture, GL_TEXTURE_2D,
                              level, 0, 0, 0, kSize >> level, kSize >> level, 1);
        EXPECT_GL_NO_ERROR();
    }
}

// Test GL_EXT_copy_image copy with a non-zero base level
TEST_P(Texture2DTestES3, CopyImageBaseLevel1)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_copy_image"));

    std::vector<GLColor> texDataBlack(8u * 8u, GLColor::black);
    std::vector<GLColor> texDataRed(4u * 4u, GLColor::red);
    std::vector<GLColor> texDataGreen(4u * 4u, GLColor::green);
    std::vector<GLColor> texDataBlue(4u * 4u, GLColor::blue);

    GLTexture srcTexture;
    GLTexture destTexture;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, destTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataBlack.data());
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, texDataRed.data());
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());
    glTexImage2D(GL_TEXTURE_2D, 3, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texDataBlue.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

    glBindTexture(GL_TEXTURE_2D, srcTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataBlack.data());
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, texDataBlue.data());
    glTexImage2D(GL_TEXTURE_2D, 3, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texDataRed.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

    glBindTexture(GL_TEXTURE_2D, 0);

    // copy
    glCopyImageSubDataEXT(srcTexture, GL_TEXTURE_2D, 1, 2, 2, 0, destTexture, GL_TEXTURE_2D, 1, 2,
                          2, 0, 2, 2, 1);

    glBindTexture(GL_TEXTURE_2D, destTexture);

    EXPECT_GL_NO_ERROR();

    glViewport(0, 0, 4, 4);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_RECT_EQ(2, 2, 2, 2, GLColor::green);
    EXPECT_PIXEL_RECT_EQ(0, 0, 4, 2, GLColor::red);
    EXPECT_PIXEL_RECT_EQ(0, 0, 2, 4, GLColor::red);
}

// Test basic GL_EXT_copy_image copy without any draw calls by attaching the texture
// to a framebuffer and reads from the framebuffer to validate the copy
TEST_P(Texture2DTestES3, CopyImageFB)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_copy_image"));

    glViewport(0, 0, 4, 4);
    std::vector<GLColor> texDataRed(4u * 4u, GLColor::red);
    GLTexture srcTexture;
    GLTexture destTexture;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, destTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, texDataRed.data());

    std::vector<GLColor> texDataGreen(4u * 4u, GLColor::green);
    glBindTexture(GL_TEXTURE_2D, srcTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    // copy
    glCopyImageSubDataEXT(srcTexture, GL_TEXTURE_2D, 0, 0, 0, 0, destTexture, GL_TEXTURE_2D, 0, 0,
                          1, 0, 3, 3, 1);

    EXPECT_GL_NO_ERROR();

    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destTexture, 0);
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    EXPECT_PIXEL_RECT_EQ(0, 1, 3, 3, GLColor::green);
    EXPECT_PIXEL_RECT_EQ(3, 0, 1, 4, GLColor::red);
    EXPECT_PIXEL_RECT_EQ(0, 0, 4, 1, GLColor::red);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Test GL_EXT_copy_image copy to a framebuffer attachment after
// invalidation. Then draw with blending onto the framebuffer.
TEST_P(Texture2DTestES3, CopyImageFBInvalidateThenBlend)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_copy_image"));

    ANGLE_GL_PROGRAM(drawBlueProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Blue());
    ANGLE_GL_PROGRAM(drawRedProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());

    glViewport(0, 0, 4, 4);
    GLTexture srcTexture;
    GLTexture textureAttachment;

    std::vector<GLColor> texDataGreen(4u * 4u, GLColor::green);
    glBindTexture(GL_TEXTURE_2D, srcTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindTexture(GL_TEXTURE_2D, textureAttachment);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureAttachment,
                           0);
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Draw something in the texture to make sure it's image is defined.
    drawQuad(drawRedProgram, essl1_shaders::PositionAttrib(), 0.0f);

    // Invalidate the framebuffer.
    const GLenum discards[] = {GL_COLOR_ATTACHMENT0};
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, discards);
    ASSERT_GL_NO_ERROR();

    // Copy into the framebuffer attachment.
    glCopyImageSubDataEXT(srcTexture, GL_TEXTURE_2D, 0, 0, 0, 0, textureAttachment, GL_TEXTURE_2D,
                          0, 0, 0, 0, 4, 4, 1);
    EXPECT_GL_NO_ERROR();

    // Draw and blend, making sure both the copy and draw happen correctly.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    drawQuad(drawBlueProgram, essl1_shaders::PositionAttrib(), 0.0f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_RECT_EQ(0, 0, 4, 4, GLColor::cyan);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Test that drawing works correctly when levels outside the BASE_LEVEL/MAX_LEVEL range do not
// have images defined.
TEST_P(Texture2DTestES3, DrawWithLevelsOutsideRangeUndefined)
{
    // Observed crashing on AMD. Oddly the crash only happens with 2D textures, not 3D or array.
    ANGLE_SKIP_TEST_IF(IsAMD() && IsOpenGL());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    std::vector<GLColor> texDataGreen(2u * 2u, GLColor::green);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that drawing works correctly when level 0 is undefined and base level is 1.
TEST_P(Texture2DTestES3, DrawWithLevelZeroUndefined)
{
    // Observed crashing on AMD. Oddly the crash only happens with 2D textures, not 3D or array.
    ANGLE_SKIP_TEST_IF(IsAMD() && IsOpenGL());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    std::vector<GLColor> texDataGreen(2u * 2u, GLColor::green);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2);

    EXPECT_GL_NO_ERROR();

    // Texture is incomplete.
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);

    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());

    // Texture is now complete.
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that drawing works correctly when levels outside the BASE_LEVEL/MAX_LEVEL range have
// dimensions that don't fit the images inside the range.
// GLES 3.0.4 section 3.8.13 Texture completeness
TEST_P(Texture2DTestES3, DrawWithLevelsOutsideRangeWithInconsistentDimensions)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    std::vector<GLColor> texDataRed(8u * 8u, GLColor::red);
    std::vector<GLColor> texDataGreen(2u * 2u, GLColor::green);
    std::vector<GLColor> texDataCyan(2u * 2u, GLColor::cyan);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // Two levels that are initially unused.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, texDataRed.data());
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataCyan.data());

    // One level that is used - only this level should affect completeness.
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    ANGLE_SKIP_TEST_IF(IsIntel() && IsWindows() && IsOpenGL());

    // Switch the level that is being used to the cyan level 2.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::cyan);
}

// Test that drawing works correctly when levels outside the BASE_LEVEL/MAX_LEVEL range do not
// have images defined.
TEST_P(Texture3DTestES3, DrawWithLevelsOutsideRangeUndefined)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, mTexture3D);
    std::vector<GLColor> texDataGreen(2u * 2u * 2u, GLColor::green);
    glTexImage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 1);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that drawing works correctly when levels outside the BASE_LEVEL/MAX_LEVEL range have
// dimensions that don't fit the images inside the range.
// GLES 3.0.4 section 3.8.13 Texture completeness
TEST_P(Texture3DTestES3, DrawWithLevelsOutsideRangeWithInconsistentDimensions)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, mTexture3D);
    std::vector<GLColor> texDataRed(8u * 8u * 8u, GLColor::red);
    std::vector<GLColor> texDataGreen(2u * 2u * 2u, GLColor::green);
    std::vector<GLColor> texDataCyan(2u * 2u * 2u, GLColor::cyan);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // Two levels that are initially unused.
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataRed.data());
    glTexImage3D(GL_TEXTURE_3D, 2, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataCyan.data());

    // One level that is used - only this level should affect completeness.
    glTexImage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 1);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    ANGLE_SKIP_TEST_IF(IsIntel() && IsWindows() && IsOpenGL());

    // Switch the level that is being used to the cyan level 2.
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 2);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 2);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::cyan);
}

// Test that drawing works correctly when levels outside the BASE_LEVEL/MAX_LEVEL range do not
// have images defined.
TEST_P(Texture2DArrayTestES3, DrawWithLevelsOutsideRangeUndefined)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m2DArrayTexture);
    std::vector<GLColor> texDataGreen(2u * 2u * 2u, GLColor::green);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 1);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that drawing works correctly when levels outside the BASE_LEVEL/MAX_LEVEL range have
// dimensions that don't fit the images inside the range.
// GLES 3.0.4 section 3.8.13 Texture completeness
TEST_P(Texture2DArrayTestES3, DrawWithLevelsOutsideRangeWithInconsistentDimensions)
{
    // TODO(crbug.com/998505): Test failing on Android FYI Release (NVIDIA Shield TV)
    ANGLE_SKIP_TEST_IF(IsNVIDIAShield());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, m2DArrayTexture);
    std::vector<GLColor> texDataRed(8u * 8u * 8u, GLColor::red);
    std::vector<GLColor> texDataGreen(2u * 2u * 2u, GLColor::green);
    std::vector<GLColor> texDataCyan(2u * 2u * 2u, GLColor::cyan);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // Two levels that are initially unused.
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataRed.data());
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 2, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataCyan.data());

    // One level that is used - only this level should affect completeness.
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 1);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    ANGLE_SKIP_TEST_IF(IsIntel() && IsWindows() && IsOpenGL());

    // Switch the level that is being used to the cyan level 2.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 2);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 2);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::cyan);
}

// Create a 2D array, then immediately redefine it to have fewer layers.  Regression test for a bug
// in the Vulkan backend where the old higher-layer-count data upload was not removed.
TEST_P(Texture2DArrayTestES3, TextureArrayRedefineThenUse)
{
    int px = getWindowWidth() / 2;
    int py = getWindowHeight() / 2;

    glBindTexture(GL_TEXTURE_2D_ARRAY, m2DArrayTexture);

    // Fill the whole texture with red, then redefine it and fill with green
    std::vector<GLColor> pixelsRed(2 * 2 * 4, GLColor::red);
    std::vector<GLColor> pixelsGreen(2 * 2 * 2, GLColor::green);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 2, 2, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelsRed.data());
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelsGreen.data());

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    EXPECT_GL_NO_ERROR();

    glUseProgram(mProgram);
    EXPECT_GL_NO_ERROR();

    // Draw the first slice
    glUniform1i(mTextureArraySliceUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::green);

    // Draw the second slice
    glUniform1i(mTextureArraySliceUniformLocation, 1);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::green);
}

// Create a 2D array texture and update layers with data and test that pruning
// of superseded updates works as expected.
TEST_P(Texture2DArrayTestES3, TextureArrayPruneSupersededUpdates)
{
    constexpr uint32_t kTexWidth  = 256;
    constexpr uint32_t kTexHeight = 256;
    constexpr uint32_t kTexLayers = 3;

    glBindTexture(GL_TEXTURE_2D_ARRAY, m2DArrayTexture);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    // Initialize entire texture.
    constexpr GLColor kInitialExpectedColor = GLColor(201u, 201u, 201u, 201u);
    std::vector<GLColor> initialData(kTexWidth * kTexHeight * kTexLayers, kInitialExpectedColor);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, kTexWidth, kTexHeight, kTexLayers, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, initialData.data());

    // Upate different layers with different colors, these together should supersed
    // the entire init update
    constexpr GLColor kExpectedColor[] = {GLColor(32u, 32u, 32u, 32u), GLColor(64u, 64u, 64u, 64u),
                                          GLColor(128u, 128u, 128u, 128u)};
    std::vector<GLColor> supersedingData[] = {
        std::vector<GLColor>(kTexWidth * kTexHeight, kExpectedColor[0]),
        std::vector<GLColor>(kTexWidth * kTexHeight, kExpectedColor[1]),
        std::vector<GLColor>(kTexWidth * kTexHeight, kExpectedColor[2])};

    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, kTexWidth, kTexHeight, 1, GL_RGBA,
                    GL_UNSIGNED_BYTE, supersedingData[0].data());
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, kTexWidth, kTexHeight, 1, GL_RGBA,
                    GL_UNSIGNED_BYTE, supersedingData[1].data());
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 2, kTexWidth, kTexHeight, 1, GL_RGBA,
                    GL_UNSIGNED_BYTE, supersedingData[2].data());

    glUseProgram(mProgram);
    EXPECT_GL_NO_ERROR();

    // Draw layer 0
    glUniform1i(mTextureArraySliceUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, kExpectedColor[0]);

    // Draw layer 1
    glUniform1i(mTextureArraySliceUniformLocation, 1);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, kExpectedColor[1]);

    // Draw layer 2
    glUniform1i(mTextureArraySliceUniformLocation, 2);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, kExpectedColor[2]);
}

// Create a 2D array, use it, then redefine it to have fewer layers.  Regression test for a bug in
// the Vulkan backend where the old higher-layer-count data upload was not removed.
TEST_P(Texture2DArrayTestES3, TextureArrayUseThenRedefineThenUse)
{
    int px = getWindowWidth() / 2;
    int py = getWindowHeight() / 2;

    glBindTexture(GL_TEXTURE_2D_ARRAY, m2DArrayTexture);

    // Fill the whole texture with red.
    std::vector<GLColor> pixelsRed(2 * 2 * 4, GLColor::red);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 2, 2, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelsRed.data());

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    EXPECT_GL_NO_ERROR();

    glUseProgram(mProgram);
    EXPECT_GL_NO_ERROR();

    // Draw the first slice
    glUniform1i(mTextureArraySliceUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::red);

    // Draw the fourth slice
    glUniform1i(mTextureArraySliceUniformLocation, 3);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::red);

    // Redefine the image and fill with green
    std::vector<GLColor> pixelsGreen(2 * 2 * 2, GLColor::green);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelsGreen.data());

    // Draw the first slice
    glUniform1i(mTextureArraySliceUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::green);

    // Draw the second slice
    glUniform1i(mTextureArraySliceUniformLocation, 1);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(px, py, GLColor::green);
}

// Test that texture completeness is updated if texture max level changes.
// GLES 3.0.4 section 3.8.13 Texture completeness
TEST_P(Texture2DTestES3, TextureCompletenessChangesWithMaxLevel)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    std::vector<GLColor> texDataGreen(8u * 8u, GLColor::green);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // A level that is initially unused.
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());

    // One level that is initially used - only this level should affect completeness.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Switch the max level to level 1. The levels within the used range now have inconsistent
    // dimensions and the texture should be incomplete.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
}

// Test that compressed textures ignore the pixel unpack state.
// (https://crbug.org/1267496)
TEST_P(Texture3DTestES3, PixelUnpackStateTexImage)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_s3tc") &&
                       !IsGLExtensionEnabled("GL_ANGLE_texture_compression_dxt3"));

    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 5);
    glBindTexture(GL_TEXTURE_2D_ARRAY, mTexture3D);

    uint8_t data[64] = {0};
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 4, 4, 4, 0, 64,
                           data);
    EXPECT_GL_NO_ERROR();
}

// Test that compressed textures ignore the pixel unpack state.
// (https://crbug.org/1267496)
TEST_P(Texture3DTestES3, PixelUnpackStateTexSubImage)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_s3tc") &&
                       !IsGLExtensionEnabled("GL_ANGLE_texture_compression_dxt3"));

    glBindTexture(GL_TEXTURE_2D_ARRAY, mTexture3D);

    uint8_t data[64] = {0};
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 4, 4, 4, 0, 64,
                           data);
    EXPECT_GL_NO_ERROR();

    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 5);

    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 4, 4, 4,
                              GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 64, data);
    EXPECT_GL_NO_ERROR();
}

// Test that 3D texture completeness is updated if texture max level changes.
// GLES 3.0.4 section 3.8.13 Texture completeness
TEST_P(Texture3DTestES3, Texture3DCompletenessChangesWithMaxLevel)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, mTexture3D);
    std::vector<GLColor> texDataGreen(2u * 2u * 2u, GLColor::green);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // A level that is initially unused.
    glTexImage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 1, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());

    // One level that is initially used - only this level should affect completeness.
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Switch the max level to level 1. The levels within the used range now have inconsistent
    // dimensions and the texture should be incomplete.
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 1);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
}

// Test that texture completeness is updated if texture base level changes.
// GLES 3.0.4 section 3.8.13 Texture completeness
TEST_P(Texture2DTestES3, TextureCompletenessChangesWithBaseLevel)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    std::vector<GLColor> texDataGreen(8u * 8u, GLColor::green);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // Two levels that are initially unused.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());

    // One level that is initially used - only this level should affect completeness.
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataGreen.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Switch the base level to level 1. The levels within the used range now have inconsistent
    // dimensions and the texture should be incomplete.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
}

// Test that texture is not complete if base level is greater than max level.
// GLES 3.0.4 section 3.8.13 Texture completeness
TEST_P(Texture2DTestES3, TextureBaseLevelGreaterThanMaxLevel)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &GLColor::green);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 10000);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    // Texture should be incomplete.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
}

// Test that immutable texture base level and max level are clamped.
// GLES 3.0.4 section 3.8.10 subsection Mipmapping
TEST_P(Texture2DTestES3, ImmutableTextureBaseLevelOutOfRange)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &GLColor::green);

    // For immutable-format textures, base level should be clamped to [0, levels - 1], and max level
    // should be clamped to [base_level, levels - 1].
    // GLES 3.0.4 section 3.8.10 subsection Mipmapping
    // In the case of this test, those rules make the effective base level and max level 0.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 10000);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 10000);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    // Texture should be complete.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that changing base level works when it affects the format of the texture.
TEST_P(Texture2DTestES3, TextureFormatChangesWithBaseLevel)
{
    // TODO(crbug.com/998505): Test failing on Android FYI Release (NVIDIA Shield TV)
    ANGLE_SKIP_TEST_IF(IsNVIDIAShield());

    ANGLE_SKIP_TEST_IF(IsIntel() && IsWindows() && IsDesktopOpenGL());

    // Observed incorrect rendering on AMD OpenGL.
    ANGLE_SKIP_TEST_IF(IsAMD() && IsDesktopOpenGL());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    std::vector<GLColor> texDataCyan(4u * 4u, GLColor::cyan);
    std::vector<GLColor> texDataGreen(4u * 4u, GLColor::green);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // RGBA8 level that's initially unused.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texDataCyan.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

    // RG8 level that's initially used, with consistent dimensions with level 0 but a different
    // format. It reads green channel data from the green and alpha channels of texDataGreen
    // (this is a bit hacky but works).
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RG8, 2, 2, 0, GL_RG, GL_UNSIGNED_BYTE, texDataGreen.data());

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Switch the texture to use the cyan level 0 with the RGBA format.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::cyan);
}

// Test that setting a texture image works when base level is out of range.
TEST_P(Texture2DTestES3, SetImageWhenBaseLevelOutOfRange)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 10000);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 10000);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &GLColor::green);

    EXPECT_GL_NO_ERROR();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);

    drawQuad(mProgram, "position", 0.5f);

    // Texture should be complete.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// In the D3D11 renderer, we need to initialize some texture formats, to fill empty channels. EG
// RBA->RGBA8, with 1.0 in the alpha channel. This test covers a bug where redefining array textures
// with these formats does not work as expected.
TEST_P(Texture2DArrayTestES3, RedefineInittableArray)
{
    std::vector<GLubyte> pixelData;
    for (size_t count = 0; count < 5000; count++)
    {
        pixelData.push_back(0u);
        pixelData.push_back(255u);
        pixelData.push_back(0u);
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, m2DArrayTexture);
    glUseProgram(mProgram);
    glUniform1i(mTextureArrayLocation, 0);

    // The first draw worked correctly.
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB, 4, 4, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 &pixelData[0]);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    drawQuad(mProgram, "position", 1.0f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // The dimension of the respecification must match the original exactly to trigger the bug.
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB, 4, 4, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 &pixelData[0]);
    drawQuad(mProgram, "position", 1.0f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    ASSERT_GL_NO_ERROR();
}

// Test shadow sampler and regular non-shadow sampler coexisting in the same shader.
// This test is needed especially to confirm that sampler registers get assigned correctly on
// the HLSL backend even when there's a mix of different HLSL sampler and texture types.
TEST_P(ShadowSamplerPlusSampler3DTestES3, ShadowSamplerPlusSampler3DDraw)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, mTexture3D);
    GLubyte texData[4];
    texData[0] = 0;
    texData[1] = 60;
    texData[2] = 0;
    texData[3] = 255;
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mTextureShadow);
    GLfloat depthTexData[1];
    depthTexData[0] = 0.5f;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
                 depthTexData);

    glUseProgram(mProgram);
    glUniform1f(mDepthRefUniformLocation, 0.3f);
    glUniform1i(mTexture3DUniformLocation, 0);
    glUniform1i(mTextureShadowUniformLocation, 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    // The shader writes 0.5 * <comparison result (1.0)> + <texture color>
    EXPECT_PIXEL_NEAR(0, 0, 128, 188, 128, 255, 2);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    // The shader writes 0.5 * <comparison result (0.0)> + <texture color>
    EXPECT_PIXEL_NEAR(0, 0, 0, 60, 0, 255, 2);
}

// Test multiple different sampler types in the same shader.
// This test makes sure that even if sampler / texture registers get grouped together based on type
// or otherwise get shuffled around in the HLSL backend of the shader translator, the D3D renderer
// still has the right register index information for each ESSL sampler.
// The tested ESSL samplers have the following types in D3D11 HLSL:
// sampler2D:         Texture2D   + SamplerState
// samplerCube:       TextureCube + SamplerState
// sampler2DShadow:   Texture2D   + SamplerComparisonState
// samplerCubeShadow: TextureCube + SamplerComparisonState
TEST_P(SamplerTypeMixTestES3, SamplerTypeMixDraw)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    GLubyte texData[4];
    texData[0] = 0;
    texData[1] = 0;
    texData[2] = 120;
    texData[3] = 255;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureCube);
    texData[0] = 0;
    texData[1] = 90;
    texData[2] = 0;
    texData[3] = 255;
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGBA8, 1, 1);
    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    texData);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, mTexture2DShadow);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    GLfloat depthTexData[1];
    depthTexData[0] = 0.5f;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
                 depthTexData);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureCubeShadow);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    depthTexData[0] = 0.2f;
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_DEPTH_COMPONENT32F, 1, 1);
    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, 0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT,
                    depthTexData);

    // http://anglebug.com/3949: TODO: Add a DS texture case

    EXPECT_GL_NO_ERROR();

    glUseProgram(mProgram);
    glUniform1f(mDepthRefUniformLocation, 0.3f);
    glUniform1i(mTexture2DUniformLocation, 0);
    glUniform1i(mTextureCubeUniformLocation, 1);
    glUniform1i(mTexture2DShadowUniformLocation, 2);
    glUniform1i(mTextureCubeShadowUniformLocation, 3);

    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    // The shader writes:
    // <texture 2d color> +
    // <cube map color> +
    // 0.25 * <comparison result (1.0)> +
    // 0.125 * <comparison result (0.0)>
    EXPECT_PIXEL_NEAR(0, 0, 64, 154, 184, 255, 2);
}

// Test different base levels on textures accessed through the same sampler array.
// Calling textureSize() on the samplers hits the D3D sampler metadata workaround.
TEST_P(TextureSizeTextureArrayTest, BaseLevelVariesInTextureArray)
{
    ANGLE_SKIP_TEST_IF(IsAMD() && IsD3D11());

    // http://anglebug.com/4391
    ANGLE_SKIP_TEST_IF(IsNVIDIA() && IsWindows() && IsD3D11());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2DA);
    GLsizei size = 64;
    for (GLint level = 0; level < 7; ++level)
    {
        ASSERT_LT(0, size);
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        size = size / 2;
    }
    ASSERT_EQ(0, size);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mTexture2DB);
    size = 128;
    for (GLint level = 0; level < 8; ++level)
    {
        ASSERT_LT(0, size);
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        size = size / 2;
    }
    ASSERT_EQ(0, size);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 3);
    EXPECT_GL_NO_ERROR();

    glUseProgram(mProgram);
    glUniform1i(mTexture0Location, 0);
    glUniform1i(mTexture1Location, 1);

    drawQuad(mProgram, essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_NO_ERROR();
    // Red channel: width of level 1 of texture A: 32.
    // Green channel: width of level 3 of texture B: 16.
    EXPECT_PIXEL_NEAR(0, 0, 32, 16, 0, 255, 2);
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DTestES3, TextureRGBImplicitAlpha1)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DTestES3, TextureRGBXImplicitAlpha1)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_rgbx_internal_format"));

    GLTexture texture2D;
    glBindTexture(GL_TEXTURE_2D, texture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBX8_ANGLE, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glActiveTexture(GL_TEXTURE0);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DTest, TextureLuminanceImplicitAlpha1)
{
    setUpProgram();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// Validate that every component of the pixel will be equal to the luminance value we've set
// and that the alpha channel will be 1 (or 255 to be exact).
TEST_P(Texture2DTest, TextureLuminanceRGBSame)
{
    setUpProgram();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    uint8_t pixel = 50;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, &pixel);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(pixel, pixel, pixel, 255));
}

// Validate that every component of the pixel will be equal to the luminance value we've set
// and that the alpha channel will be the second component.
TEST_P(Texture2DTest, TextureLuminanceAlphaRGBSame)
{
    setUpProgram();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    uint8_t pixel[] = {50, 25};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 1, 1, 0, GL_LUMINANCE_ALPHA,
                 GL_UNSIGNED_BYTE, pixel);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor(pixel[0], pixel[0], pixel[0], pixel[1]));
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DTest, TextureLuminance32ImplicitAlpha1)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float"));
    ANGLE_SKIP_TEST_IF(IsD3D9());
    ANGLE_SKIP_TEST_IF(IsVulkan());

    setUpProgram();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1, 1, 0, GL_LUMINANCE, GL_FLOAT, nullptr);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DTest, TextureLuminance16ImplicitAlpha1)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float"));
    ANGLE_SKIP_TEST_IF(IsD3D9());
    ANGLE_SKIP_TEST_IF(IsVulkan());
    // TODO(ynovikov): re-enable once root cause of http://anglebug.com/1420 is fixed
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsAdreno() && IsOpenGLES());

    setUpProgram();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1, 1, 0, GL_LUMINANCE, GL_HALF_FLOAT_OES, nullptr);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// Test that CopyTexImage2D does not trigger assertion after CompressedTexImage2D.
// https://crbug.com/1216276
TEST_P(Texture2DTest, CopyAfterCompressed)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_dxt1"));

    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 4, 4, 0, 8, nullptr);
    EXPECT_GL_NO_ERROR();

    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 0, 0, 4, 4, 0);
    EXPECT_GL_NO_ERROR();
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DUnsignedIntegerAlpha1TestES3, TextureRGB8UIImplicitAlpha1)
{
    ANGLE_SKIP_TEST_IF(IsIntel() && IsOSX());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8UI, 1, 1, 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DIntegerAlpha1TestES3, TextureRGB8IImplicitAlpha1)
{
    ANGLE_SKIP_TEST_IF(IsIntel() && IsOSX());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8I, 1, 1, 0, GL_RGB_INTEGER, GL_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DUnsignedIntegerAlpha1TestES3, TextureRGB16UIImplicitAlpha1)
{
    ANGLE_SKIP_TEST_IF(IsIntel() && IsOSX());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16UI, 1, 1, 0, GL_RGB_INTEGER, GL_UNSIGNED_SHORT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DIntegerAlpha1TestES3, TextureRGB16IImplicitAlpha1)
{
    ANGLE_SKIP_TEST_IF(IsIntel() && IsOSX());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16I, 1, 1, 0, GL_RGB_INTEGER, GL_SHORT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DUnsignedIntegerAlpha1TestES3, TextureRGB32UIImplicitAlpha1)
{
    ANGLE_SKIP_TEST_IF(IsIntel() && IsOSX());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32UI, 1, 1, 0, GL_RGB_INTEGER, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DIntegerAlpha1TestES3, TextureRGB32IImplicitAlpha1)
{
    ANGLE_SKIP_TEST_IF(IsIntel() && IsOSX());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32I, 1, 1, 0, GL_RGB_INTEGER, GL_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DTestES3, TextureRGBSNORMImplicitAlpha1)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8_SNORM, 1, 1, 0, GL_RGB, GL_BYTE, nullptr);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DTestES3, TextureRGB9E5ImplicitAlpha1)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB9_E5, 1, 1, 0, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV,
                 nullptr);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DTestES3, TextureCOMPRESSEDRGB8ETC2ImplicitAlpha1)
{
    // ETC texture formats are not supported on Mac OpenGL. http://anglebug.com/3853
    ANGLE_SKIP_TEST_IF(IsOSX() && IsDesktopOpenGL());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB8_ETC2, 1, 1, 0, 8, nullptr);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// When sampling a texture without an alpha channel, "1" is returned as the alpha value.
// ES 3.0.4 table 3.24
TEST_P(Texture2DTestES3, TextureCOMPRESSEDSRGB8ETC2ImplicitAlpha1)
{
    // ETC texture formats are not supported on Mac OpenGL. http://anglebug.com/3853
    ANGLE_SKIP_TEST_IF(IsOSX() && IsDesktopOpenGL());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_SRGB8_ETC2, 1, 1, 0, 8, nullptr);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// ETC2 punchthrough alpha formats must be initialized to opaque black when emulated
// http://anglebug.com/6936
TEST_P(Texture2DTestES3RobustInit, TextureCOMPRESSEDRGB8A1ETC2)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, 1, 1, 0,
                           8, nullptr);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// ETC2 punchthrough alpha formats must be initialized to opaque black when emulated
// http://anglebug.com/6936
TEST_P(Texture2DTestES3RobustInit, TextureCOMPRESSEDSRGB8A1ETC2)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, 1, 1, 0,
                           8, nullptr);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_ALPHA_EQ(0, 0, 255);
}

// Test that compressed textures ignore the pixel unpack state.
// (https://crbug.org/1267496)
TEST_P(Texture2DTestES3, PixelUnpackStateTexImage)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_s3tc") &&
                       !IsGLExtensionEnabled("GL_ANGLE_texture_compression_dxt3"));

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 9);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    uint8_t data[64] = {0};
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 8, 8, 0, 64, data);
    EXPECT_GL_NO_ERROR();
}

// Test that compressed textures ignore the pixel unpack state.
// (https://crbug.org/1267496)
TEST_P(Texture2DTestES3, PixelUnpackStateTexSubImage)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_s3tc") &&
                       !IsGLExtensionEnabled("GL_ANGLE_texture_compression_dxt3"));

    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    uint8_t data[64] = {0};
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 8, 8, 0, 64, data);
    EXPECT_GL_NO_ERROR();

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 9);

    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 64,
                              data);
    EXPECT_GL_NO_ERROR();
}

// Test for http://anglebug.com/6926.
TEST_P(Texture2DTestES3, TextureRGBUpdateWithPBO)
{
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Texture2D(), essl1_shaders::fs::Texture2D());

    glViewport(0, 0, 16, 16);

    GLTexture tex1;
    std::vector<GLColor> texDataRed(16u * 16u, GLColor::red);
    std::vector<GLColor> texDataGreen(16u * 16u, GLColor::green);

    glBindTexture(GL_TEXTURE_2D, tex1);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, 16, 16);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 16, 16, GL_RGB, GL_UNSIGNED_BYTE, texDataRed.data());
    ASSERT_GL_NO_ERROR();

    GLBuffer pbo;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 3 * 16 * 16, texDataGreen.data(), GL_STATIC_DRAW);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 16, 16, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    drawQuad(program, essl1_shaders::PositionAttrib(), 0.0f);
    ASSERT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    EXPECT_PIXEL_COLOR_EQ(4, 4, GLColor::green);
}

// Copied from Texture2DTest::TexStorage
// Test that glTexSubImage2D works properly when glTexStorage2DEXT has initialized the image with a
// default color.
TEST_P(Texture2DTestES31PPO, TexStorage)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());
    ANGLE_SKIP_TEST_IF((getClientMajorVersion() < 3 && getClientMinorVersion() < 1) &&
                       !IsGLExtensionEnabled("GL_EXT_texture_storage"));

    const char *vertexShaderSource   = getVertexShaderSource();
    const char *fragmentShaderSource = getFragmentShaderSource();

    bindProgramPipeline(vertexShaderSource, fragmentShaderSource);
    mTexture2DUniformLocation = glGetUniformLocation(mFragProg, getTextureUniformName());

    int width  = getWindowWidth();
    int height = getWindowHeight();

    GLTexture tex2D;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex2D);

    // Fill with red
    std::vector<GLubyte> pixels(3 * 16 * 16);
    for (size_t pixelId = 0; pixelId < 16 * 16; ++pixelId)
    {
        pixels[pixelId * 3 + 0] = 255;
        pixels[pixelId * 3 + 1] = 0;
        pixels[pixelId * 3 + 2] = 0;
    }

    // ANGLE internally uses RGBA as the internal format for RGB images, therefore glTexStorage2DEXT
    // initializes the image to a default color to get a consistent alpha color. The data is kept in
    // a CPU-side image and the image is marked as dirty.
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, 16, 16);

    // Initializes the color of the upper-left 8x8 pixels, leaves the other pixels untouched.
    // glTexSubImage2D should take into account that the image is dirty.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glActiveShaderProgram(mPipeline, mFragProg);
    glUniform1i(mTexture2DUniformLocation, 0);

    std::array<Vector3, 6> quadVertices = ANGLETestBase::GetQuadVertices();
    ppoDrawQuad(quadVertices, "position", 0.5f, 1.0f);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(width / 4, height / 4, 255, 0, 0, 255);

    // Validate that the region of the texture without data has an alpha of 1.0
    angle::GLColor pixel = ReadColor(3 * width / 4, 3 * height / 4);
    EXPECT_EQ(255, pixel.A);
}

// Copied from Texture2DTestES3::SingleTextureMultipleSamplers
// Tests behaviour with a single texture and multiple sampler objects.
TEST_P(Texture2DTestES31PPO, SingleTextureMultipleSamplers)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    const char *vertexShaderSource   = getVertexShaderSource();
    const char *fragmentShaderSource = getFragmentShaderSource();

    bindProgramPipeline(vertexShaderSource, fragmentShaderSource);
    mTexture2DUniformLocation = glGetUniformLocation(mFragProg, getTextureUniformName());

    GLint maxTextureUnits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
    ANGLE_SKIP_TEST_IF(maxTextureUnits < 4);

    constexpr int kSize                 = 16;
    std::array<Vector3, 6> quadVertices = ANGLETestBase::GetQuadVertices();

    // Make a single-level texture, fill it with red.
    std::vector<GLColor> redColors(kSize * kSize, GLColor::red);
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 redColors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Simple confidence check.
    bind2DTexturedQuadProgramPipeline();
    ppoDrawQuad(quadVertices, "position", 0.5f, 1.0f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Bind texture to unit 1 with a sampler object making it incomplete.
    GLSampler sampler;
    glBindSampler(0, sampler);
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Make a mipmap texture, fill it with blue.
    std::vector<GLColor> blueColors(kSize * kSize, GLColor::blue);
    GLTexture mipmapTex;
    glBindTexture(GL_TEXTURE_2D, mipmapTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 blueColors.data());
    glGenerateMipmap(GL_TEXTURE_2D);

    // Draw with the sampler, expect blue.
    draw2DTexturedQuad(0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    // Simple multitexturing program.
    constexpr char kVS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec2 position;\n"
        "out vec2 texCoord;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 0, 1);\n"
        "    texCoord = position * 0.5 + vec2(0.5);\n"
        "}";

    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec2 texCoord;\n"
        "uniform sampler2D tex1;\n"
        "uniform sampler2D tex2;\n"
        "uniform sampler2D tex3;\n"
        "uniform sampler2D tex4;\n"
        "out vec4 color;\n"
        "void main()\n"
        "{\n"
        "    color = (texture(tex1, texCoord) + texture(tex2, texCoord) \n"
        "          +  texture(tex3, texCoord) + texture(tex4, texCoord)) * 0.25;\n"
        "}";

    bindProgramPipeline(kVS, kFS);

    std::array<GLint, 4> texLocations = {
        {glGetUniformLocation(mFragProg, "tex1"), glGetUniformLocation(mFragProg, "tex2"),
         glGetUniformLocation(mFragProg, "tex3"), glGetUniformLocation(mFragProg, "tex4")}};
    for (GLint location : texLocations)
    {
        ASSERT_NE(-1, location);
    }

    // Init the uniform data.
    glActiveShaderProgram(mPipeline, mFragProg);
    for (GLint location = 0; location < 4; ++location)
    {
        glUniform1i(texLocations[location], location);
    }

    // Initialize four samplers
    GLSampler samplers[4];

    // 0: non-mipped.
    glBindSampler(0, samplers[0]);
    glSamplerParameteri(samplers[0], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(samplers[0], GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 1: mipped.
    glBindSampler(1, samplers[1]);
    glSamplerParameteri(samplers[1], GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glSamplerParameteri(samplers[1], GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 2: non-mipped.
    glBindSampler(2, samplers[2]);
    glSamplerParameteri(samplers[2], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(samplers[2], GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 3: mipped.
    glBindSampler(3, samplers[3]);
    glSamplerParameteri(samplers[3], GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glSamplerParameteri(samplers[3], GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Bind two blue mipped textures and two single layer textures, should all draw.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mipmapTex);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, mipmapTex);

    ASSERT_GL_NO_ERROR();

    ppoDrawQuad(quadVertices, "position", 0.5f, 1.0f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(0, 0, 128, 0, 128, 255, 2);

    // Bind four single layer textures, two should be incomplete.
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, tex);

    ppoDrawQuad(quadVertices, "position", 0.5f, 1.0f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(0, 0, 128, 0, 0, 255, 2);
}

// Use a sampler in a uniform struct.
TEST_P(SamplerInStructTest, SamplerInStruct)
{
    runSamplerInStructTest();
}

// Use a sampler in a uniform struct that's passed as a function parameter.
TEST_P(SamplerInStructAsFunctionParameterTest, SamplerInStructAsFunctionParameter)
{
    // Fails on Nexus 5X due to a driver bug. http://anglebug.com/1427
    ANGLE_SKIP_TEST_IF(IsNexus5X() && IsOpenGLES());

    runSamplerInStructTest();
}

// Use a sampler in a uniform struct array with a struct from the array passed as a function
// parameter.
TEST_P(SamplerInStructArrayAsFunctionParameterTest, SamplerInStructArrayAsFunctionParameter)
{
    // Fails on Nexus 5X due to a driver bug. http://anglebug.com/1427
    ANGLE_SKIP_TEST_IF(IsNexus5X() && IsOpenGLES());

    runSamplerInStructTest();
}

// Use a sampler in a struct inside a uniform struct with the nested struct passed as a function
// parameter.
TEST_P(SamplerInNestedStructAsFunctionParameterTest, SamplerInNestedStructAsFunctionParameter)
{
    // Fails on Nexus 5X due to a driver bug. http://anglebug.com/1427
    ANGLE_SKIP_TEST_IF(IsNexus5X() && IsOpenGLES());

    runSamplerInStructTest();
}

// Make sure that there isn't a name conflict between sampler extracted from a struct and a
// similarly named uniform.
TEST_P(SamplerInStructAndOtherVariableTest, SamplerInStructAndOtherVariable)
{
    runSamplerInStructTest();
}

// GL_EXT_texture_filter_anisotropic
class TextureAnisotropyTest : public Texture2DTest
{
  protected:
    void uploadTexture()
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTexture2D);
        GLColor texDataRed[1] = {GLColor::red};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texDataRed);
        EXPECT_GL_NO_ERROR();
    }
};

// Tests that setting anisotropic filtering doesn't cause failures at draw time.
TEST_P(TextureAnisotropyTest, AnisotropyFunctional)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_filter_anisotropic"));

    setUpProgram();

    uploadTexture();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2.0f);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::red);
}

// GL_OES_texture_border_clamp
class TextureBorderClampTest : public Texture2DTest
{
  protected:
    TextureBorderClampTest() : Texture2DTest() {}

    const char *getVertexShaderSource() override
    {
        return
            R"(precision highp float;
            attribute vec4 position;
            varying vec2 texcoord;

            void main()
            {
                gl_Position = vec4(position.xy, 0.0, 1.0);
                // texcoords in [-0.5, 1.5]
                texcoord = (position.xy) + 0.5;
            })";
    }

    void uploadTexture()
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTexture2D);
        std::vector<GLColor> texDataRed(1, GLColor::red);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     texDataRed.data());
        EXPECT_GL_NO_ERROR();
    }
};

// Test if the color set as GL_TEXTURE_BORDER_COLOR is used when sampling outside of the texture in
// GL_CLAMP_TO_BORDER wrap mode (set with glTexParameter).
TEST_P(TextureBorderClampTest, TextureBorderClampFunctional)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_border_clamp"));

    setUpProgram();

    uploadTexture();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &kFloatGreen.R);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green);
}

// Test reading back GL_TEXTURE_BORDER_COLOR by glGetTexParameter.
TEST_P(TextureBorderClampTest, TextureBorderClampFunctional2)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_border_clamp"));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &kFloatGreen.R);

    GLint colorFixedPoint[4] = {0};
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, colorFixedPoint);
    constexpr GLint colorGreenFixedPoint[4] = {0, std::numeric_limits<GLint>::max(), 0,
                                               std::numeric_limits<GLint>::max()};
    EXPECT_EQ(colorFixedPoint[0], colorGreenFixedPoint[0]);
    EXPECT_EQ(colorFixedPoint[1], colorGreenFixedPoint[1]);
    EXPECT_EQ(colorFixedPoint[2], colorGreenFixedPoint[2]);
    EXPECT_EQ(colorFixedPoint[3], colorGreenFixedPoint[3]);

    constexpr GLint colorBlueFixedPoint[4] = {0, 0, std::numeric_limits<GLint>::max(),
                                              std::numeric_limits<GLint>::max()};
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, colorBlueFixedPoint);

    GLfloat color[4] = {0.0f};
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
    EXPECT_EQ(color[0], kFloatBlue.R);
    EXPECT_EQ(color[1], kFloatBlue.G);
    EXPECT_EQ(color[2], kFloatBlue.B);
    EXPECT_EQ(color[3], kFloatBlue.A);
}

// Test GL_TEXTURE_BORDER_COLOR parameter validation at glTexParameter.
TEST_P(TextureBorderClampTest, TextureBorderClampValidation)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_border_clamp"));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, 1.0f);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, std::numeric_limits<GLint>::max());
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glTexParameterfv(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_BORDER_COLOR, &kFloatGreen.R);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    GLint colorInt[4] = {0};
    glTexParameteriv(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_TEXTURE_BORDER_COLOR, colorInt);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (getClientMajorVersion() < 3)
    {
        glTexParameterIivOES(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, colorInt);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
        glGetTexParameterIivOES(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, colorInt);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        GLuint colorUInt[4] = {0};
        glTexParameterIuivOES(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, colorUInt);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
        glGetTexParameterIuivOES(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, colorUInt);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        GLSampler sampler;
        glSamplerParameterIivOES(sampler, GL_TEXTURE_BORDER_COLOR, colorInt);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
        glGetSamplerParameterIivOES(sampler, GL_TEXTURE_BORDER_COLOR, colorInt);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        glSamplerParameterIuivOES(sampler, GL_TEXTURE_BORDER_COLOR, colorUInt);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
        glGetSamplerParameterIuivOES(sampler, GL_TEXTURE_BORDER_COLOR, colorUInt);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
}

class TextureBorderClampTestES3 : public TextureBorderClampTest
{
  protected:
    TextureBorderClampTestES3() : TextureBorderClampTest() {}
};

// Test if the color set as GL_TEXTURE_BORDER_COLOR is used when sampling outside of the texture in
// GL_CLAMP_TO_BORDER wrap mode (set with glSamplerParameter).
TEST_P(TextureBorderClampTestES3, TextureBorderClampES3Functional)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_border_clamp"));

    setUpProgram();

    uploadTexture();

    GLSampler sampler;
    glBindSampler(0, sampler);
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, &kFloatGreen.R);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green);
}

// Test reading back GL_TEXTURE_BORDER_COLOR by glGetSamplerParameter.
TEST_P(TextureBorderClampTestES3, TextureBorderClampES3Functional2)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_border_clamp"));

    glActiveTexture(GL_TEXTURE0);

    GLSampler sampler;
    glBindSampler(0, sampler);

    glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, &kFloatGreen.R);

    GLint colorFixedPoint[4] = {0};
    glGetSamplerParameteriv(sampler, GL_TEXTURE_BORDER_COLOR, colorFixedPoint);
    constexpr GLint colorGreenFixedPoint[4] = {0, std::numeric_limits<GLint>::max(), 0,
                                               std::numeric_limits<GLint>::max()};
    EXPECT_EQ(colorFixedPoint[0], colorGreenFixedPoint[0]);
    EXPECT_EQ(colorFixedPoint[1], colorGreenFixedPoint[1]);
    EXPECT_EQ(colorFixedPoint[2], colorGreenFixedPoint[2]);
    EXPECT_EQ(colorFixedPoint[3], colorGreenFixedPoint[3]);

    constexpr GLint colorBlueFixedPoint[4] = {0, 0, std::numeric_limits<GLint>::max(),
                                              std::numeric_limits<GLint>::max()};
    glSamplerParameteriv(sampler, GL_TEXTURE_BORDER_COLOR, colorBlueFixedPoint);

    GLfloat color[4] = {0.0f};
    glGetSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, color);
    EXPECT_EQ(color[0], kFloatBlue.R);
    EXPECT_EQ(color[1], kFloatBlue.G);
    EXPECT_EQ(color[2], kFloatBlue.B);
    EXPECT_EQ(color[3], kFloatBlue.A);

    constexpr GLint colorSomewhatRedInt[4] = {500000, 0, 0, std::numeric_limits<GLint>::max()};
    glSamplerParameterIivOES(sampler, GL_TEXTURE_BORDER_COLOR, colorSomewhatRedInt);
    GLint colorInt[4] = {0};
    glGetSamplerParameterIivOES(sampler, GL_TEXTURE_BORDER_COLOR, colorInt);
    EXPECT_EQ(colorInt[0], colorSomewhatRedInt[0]);
    EXPECT_EQ(colorInt[1], colorSomewhatRedInt[1]);
    EXPECT_EQ(colorInt[2], colorSomewhatRedInt[2]);
    EXPECT_EQ(colorInt[3], colorSomewhatRedInt[3]);

    constexpr GLuint colorSomewhatRedUInt[4] = {500000, 0, 0, std::numeric_limits<GLuint>::max()};
    glSamplerParameterIuivOES(sampler, GL_TEXTURE_BORDER_COLOR, colorSomewhatRedUInt);
    GLuint colorUInt[4] = {0};
    glGetSamplerParameterIuivOES(sampler, GL_TEXTURE_BORDER_COLOR, colorUInt);
    EXPECT_EQ(colorUInt[0], colorSomewhatRedUInt[0]);
    EXPECT_EQ(colorUInt[1], colorSomewhatRedUInt[1]);
    EXPECT_EQ(colorUInt[2], colorSomewhatRedUInt[2]);
    EXPECT_EQ(colorUInt[3], colorSomewhatRedUInt[3]);

    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    constexpr GLint colorSomewhatGreenInt[4] = {0, 500000, 0, std::numeric_limits<GLint>::max()};
    glTexParameterIivOES(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, colorSomewhatGreenInt);
    glGetTexParameterIivOES(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, colorInt);
    EXPECT_EQ(colorInt[0], colorSomewhatGreenInt[0]);
    EXPECT_EQ(colorInt[1], colorSomewhatGreenInt[1]);
    EXPECT_EQ(colorInt[2], colorSomewhatGreenInt[2]);
    EXPECT_EQ(colorInt[3], colorSomewhatGreenInt[3]);

    constexpr GLuint colorSomewhatGreenUInt[4] = {0, 500000, 0, std::numeric_limits<GLuint>::max()};
    glTexParameterIuivOES(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, colorSomewhatGreenUInt);
    glGetTexParameterIuivOES(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, colorUInt);
    EXPECT_EQ(colorUInt[0], colorSomewhatGreenUInt[0]);
    EXPECT_EQ(colorUInt[1], colorSomewhatGreenUInt[1]);
    EXPECT_EQ(colorUInt[2], colorSomewhatGreenUInt[2]);
    EXPECT_EQ(colorUInt[3], colorSomewhatGreenUInt[3]);
}

// Test GL_TEXTURE_BORDER_COLOR parameter validation at glSamplerParameter.
TEST_P(TextureBorderClampTestES3, TextureBorderClampES3Validation)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_border_clamp"));

    glActiveTexture(GL_TEXTURE0);

    GLSampler sampler;
    glBindSampler(0, sampler);

    glSamplerParameterf(sampler, GL_TEXTURE_BORDER_COLOR, 1.0f);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glSamplerParameteri(sampler, GL_TEXTURE_BORDER_COLOR, std::numeric_limits<GLint>::max());
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

class TextureBorderClampIntegerTestES3 : public Texture2DTest
{
  protected:
    TextureBorderClampIntegerTestES3() : Texture2DTest(), isUnsignedIntTest(false) {}

    const char *getVertexShaderSource() override
    {
        return
            R"(#version 300 es
            out vec2 texcoord;
            in vec4 position;

            void main()
            {
                gl_Position = vec4(position.xy, 0.0, 1.0);
                // texcoords in [-0.5, 1.5]
                texcoord = (position.xy) + 0.5;
            })";
    }

    const char *getFragmentShaderSource() override
    {
        if (isUnsignedIntTest)
        {
            return "#version 300 es\n"
                   "precision highp float;\n"
                   "uniform highp usampler2D tex;\n"
                   "in vec2 texcoord;\n"
                   "out vec4 fragColor;\n"

                   "void main()\n"
                   "{\n"
                   "vec4 red   = vec4(1.0, 0.0, 0.0, 1.0);\n"
                   "vec4 green = vec4(0.0, 1.0, 0.0, 1.0);\n"
                   "fragColor = (texture(tex, texcoord).r == 150u)"
                   "            ? green : red;\n"
                   "}\n";
        }
        else
        {
            return "#version 300 es\n"
                   "precision highp float;\n"
                   "uniform highp isampler2D tex;\n"
                   "in vec2 texcoord;\n"
                   "out vec4 fragColor;\n"

                   "void main()\n"
                   "{\n"
                   "vec4 red   = vec4(1.0, 0.0, 0.0, 1.0);\n"
                   "vec4 green = vec4(0.0, 1.0, 0.0, 1.0);\n"
                   "fragColor = (texture(tex, texcoord).r == -50)"
                   "            ? green : red;\n"
                   "}\n";
        }
    }

    void uploadTexture()
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTexture2D);
        if (isUnsignedIntTest)
        {
            std::vector<GLubyte> texData(4, 100);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 1, 1, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,
                         texData.data());
        }
        else
        {
            std::vector<GLbyte> texData(4, 100);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8I, 1, 1, 0, GL_RGBA_INTEGER, GL_BYTE,
                         texData.data());
        }
        EXPECT_GL_NO_ERROR();
    }

    bool isUnsignedIntTest;
};

// Test if the integer values set as GL_TEXTURE_BORDER_COLOR is used when sampling outside of the
// integer texture in GL_CLAMP_TO_BORDER wrap mode (set with glTexParameterIivOES).
TEST_P(TextureBorderClampIntegerTestES3, TextureBorderClampInteger)
{
    // Fails on Win10 FYI x64 Release (AMD RX 550). http://anglebug.com/3760
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && IsDesktopOpenGL());

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_border_clamp"));

    setUpProgram();

    uploadTexture();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    constexpr GLint borderColor[4] = {-50, -50, -50, -50};
    glTexParameterIivOES(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green);
}

// Test if the integer values set as GL_TEXTURE_BORDER_COLOR is used when sampling outside of the
// integer texture in GL_CLAMP_TO_BORDER wrap mode (set with glTexParameterIivOES).
TEST_P(TextureBorderClampIntegerTestES3, TextureBorderClampInteger2)
{
    // Fails on Win10 FYI x64 Release (AMD RX 550). http://anglebug.com/3760
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && IsDesktopOpenGL());

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_border_clamp"));

    setUpProgram();

    uploadTexture();

    GLSampler sampler;
    glBindSampler(0, sampler);
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    constexpr GLint borderColor[4] = {-50, -50, -50, -50};
    glSamplerParameterIivOES(sampler, GL_TEXTURE_BORDER_COLOR, borderColor);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green);
}

// Test if the unsigned integer values set as GL_TEXTURE_BORDER_COLOR is used when sampling outside
// of the unsigned integer texture in GL_CLAMP_TO_BORDER wrap mode (set with glTexParameterIuivOES).
TEST_P(TextureBorderClampIntegerTestES3, TextureBorderClampIntegerUnsigned)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_border_clamp"));

    isUnsignedIntTest = true;

    setUpProgram();

    uploadTexture();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    constexpr GLuint borderColor[4] = {150, 150, 150, 150};
    glTexParameterIuivOES(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green);
}

// Test if the unsigned integer values set as GL_TEXTURE_BORDER_COLOR is used when sampling outside
// of the unsigned integer texture in GL_CLAMP_TO_BORDER wrap mode (set with
// glSamplerParameterIuivOES).
TEST_P(TextureBorderClampIntegerTestES3, TextureBorderClampIntegerUnsigned2)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_border_clamp"));

    isUnsignedIntTest = true;

    setUpProgram();

    uploadTexture();

    GLSampler sampler;
    glBindSampler(0, sampler);
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    constexpr GLuint borderColor[4] = {150, 150, 150, 150};
    glSamplerParameterIuivOES(sampler, GL_TEXTURE_BORDER_COLOR, borderColor);

    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green);
}

// ~GL_OES_texture_border_clamp

class TextureLimitsTest : public ANGLETest<>
{
  protected:
    struct RGBA8
    {
        uint8_t R, G, B, A;
    };

    TextureLimitsTest()
        : mProgram(0), mMaxVertexTextures(0), mMaxFragmentTextures(0), mMaxCombinedTextures(0)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testSetUp() override
    {
        glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &mMaxVertexTextures);
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &mMaxFragmentTextures);
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &mMaxCombinedTextures);

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        if (mProgram != 0)
        {
            glDeleteProgram(mProgram);
            mProgram = 0;

            if (!mTextures.empty())
            {
                glDeleteTextures(static_cast<GLsizei>(mTextures.size()), &mTextures[0]);
            }
        }
    }

    void compileProgramWithTextureCounts(const std::string &vertexPrefix,
                                         GLint vertexTextureCount,
                                         GLint vertexActiveTextureCount,
                                         const std::string &fragPrefix,
                                         GLint fragmentTextureCount,
                                         GLint fragmentActiveTextureCount)
    {
        std::stringstream vertexShaderStr;
        vertexShaderStr << "attribute vec2 position;\n"
                        << "varying vec4 color;\n"
                        << "varying vec2 texCoord;\n";

        for (GLint textureIndex = 0; textureIndex < vertexTextureCount; ++textureIndex)
        {
            vertexShaderStr << "uniform sampler2D " << vertexPrefix << textureIndex << ";\n";
        }

        vertexShaderStr << "void main() {\n"
                        << "  gl_Position = vec4(position, 0, 1);\n"
                        << "  texCoord = (position * 0.5) + 0.5;\n"
                        << "  color = vec4(0);\n";

        for (GLint textureIndex = 0; textureIndex < vertexActiveTextureCount; ++textureIndex)
        {
            vertexShaderStr << "  color += texture2D(" << vertexPrefix << textureIndex
                            << ", texCoord);\n";
        }

        vertexShaderStr << "}";

        std::stringstream fragmentShaderStr;
        fragmentShaderStr << "varying mediump vec4 color;\n"
                          << "varying mediump vec2 texCoord;\n";

        for (GLint textureIndex = 0; textureIndex < fragmentTextureCount; ++textureIndex)
        {
            fragmentShaderStr << "uniform sampler2D " << fragPrefix << textureIndex << ";\n";
        }

        fragmentShaderStr << "void main() {\n"
                          << "  gl_FragColor = color;\n";

        for (GLint textureIndex = 0; textureIndex < fragmentActiveTextureCount; ++textureIndex)
        {
            fragmentShaderStr << "  gl_FragColor += texture2D(" << fragPrefix << textureIndex
                              << ", texCoord);\n";
        }

        fragmentShaderStr << "}";

        const std::string &vertexShaderSource   = vertexShaderStr.str();
        const std::string &fragmentShaderSource = fragmentShaderStr.str();

        mProgram = CompileProgram(vertexShaderSource.c_str(), fragmentShaderSource.c_str());
    }

    RGBA8 getPixel(GLint texIndex)
    {
        RGBA8 pixel = {static_cast<uint8_t>(texIndex & 0x7u), static_cast<uint8_t>(texIndex >> 3),
                       0, 255u};
        return pixel;
    }

    void initTextures(GLint tex2DCount, GLint texCubeCount)
    {
        GLint totalCount = tex2DCount + texCubeCount;
        mTextures.assign(totalCount, 0);
        glGenTextures(totalCount, &mTextures[0]);
        ASSERT_GL_NO_ERROR();

        std::vector<RGBA8> texData(16 * 16);

        GLint texIndex = 0;
        for (; texIndex < tex2DCount; ++texIndex)
        {
            texData.assign(texData.size(), getPixel(texIndex));
            glActiveTexture(GL_TEXTURE0 + texIndex);
            glBindTexture(GL_TEXTURE_2D, mTextures[texIndex]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         &texData[0]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        ASSERT_GL_NO_ERROR();

        for (; texIndex < texCubeCount; ++texIndex)
        {
            texData.assign(texData.size(), getPixel(texIndex));
            glActiveTexture(GL_TEXTURE0 + texIndex);
            glBindTexture(GL_TEXTURE_CUBE_MAP, mTextures[texIndex]);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 16, 16, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, &texData[0]);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, 16, 16, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, &texData[0]);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, 16, 16, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, &texData[0]);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, 16, 16, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, &texData[0]);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, 16, 16, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, &texData[0]);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, 16, 16, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, &texData[0]);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        ASSERT_GL_NO_ERROR();
    }

    void testWithTextures(GLint vertexTextureCount,
                          const std::string &vertexTexturePrefix,
                          GLint fragmentTextureCount,
                          const std::string &fragmentTexturePrefix)
    {
        // Generate textures
        initTextures(vertexTextureCount + fragmentTextureCount, 0);

        glUseProgram(mProgram);
        RGBA8 expectedSum = {0};
        for (GLint texIndex = 0; texIndex < vertexTextureCount; ++texIndex)
        {
            std::stringstream uniformNameStr;
            uniformNameStr << vertexTexturePrefix << texIndex;
            const std::string &uniformName = uniformNameStr.str();
            GLint location                 = glGetUniformLocation(mProgram, uniformName.c_str());
            ASSERT_NE(-1, location);

            glUniform1i(location, texIndex);
            RGBA8 contribution = getPixel(texIndex);
            expectedSum.R += contribution.R;
            expectedSum.G += contribution.G;
        }

        for (GLint texIndex = 0; texIndex < fragmentTextureCount; ++texIndex)
        {
            std::stringstream uniformNameStr;
            uniformNameStr << fragmentTexturePrefix << texIndex;
            const std::string &uniformName = uniformNameStr.str();
            GLint location                 = glGetUniformLocation(mProgram, uniformName.c_str());
            ASSERT_NE(-1, location);

            glUniform1i(location, texIndex + vertexTextureCount);
            RGBA8 contribution = getPixel(texIndex + vertexTextureCount);
            expectedSum.R += contribution.R;
            expectedSum.G += contribution.G;
        }

        ASSERT_GE(256u, expectedSum.G);

        drawQuad(mProgram, "position", 0.5f);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_NEAR(0, 0, expectedSum.R, expectedSum.G, 0, 255, 1);
    }

    GLuint mProgram;
    std::vector<GLuint> mTextures;
    GLint mMaxVertexTextures;
    GLint mMaxFragmentTextures;
    GLint mMaxCombinedTextures;
};

// Test rendering with the maximum vertex texture units.
TEST_P(TextureLimitsTest, MaxVertexTextures)
{
    compileProgramWithTextureCounts("tex", mMaxVertexTextures, mMaxVertexTextures, "tex", 0, 0);
    ASSERT_NE(0u, mProgram);
    ASSERT_GL_NO_ERROR();

    testWithTextures(mMaxVertexTextures, "tex", 0, "tex");
}

// Test rendering with the maximum fragment texture units.
TEST_P(TextureLimitsTest, MaxFragmentTextures)
{
    compileProgramWithTextureCounts("tex", 0, 0, "tex", mMaxFragmentTextures, mMaxFragmentTextures);
    ASSERT_NE(0u, mProgram);
    ASSERT_GL_NO_ERROR();

    testWithTextures(mMaxFragmentTextures, "tex", 0, "tex");
}

// Test rendering with maximum combined texture units.
TEST_P(TextureLimitsTest, MaxCombinedTextures)
{
    GLint vertexTextures = mMaxVertexTextures;

    if (vertexTextures + mMaxFragmentTextures > mMaxCombinedTextures)
    {
        vertexTextures = mMaxCombinedTextures - mMaxFragmentTextures;
    }

    compileProgramWithTextureCounts("vtex", vertexTextures, vertexTextures, "ftex",
                                    mMaxFragmentTextures, mMaxFragmentTextures);
    ASSERT_NE(0u, mProgram);
    ASSERT_GL_NO_ERROR();

    testWithTextures(vertexTextures, "vtex", mMaxFragmentTextures, "ftex");
}

// Negative test for exceeding the number of vertex textures
TEST_P(TextureLimitsTest, ExcessiveVertexTextures)
{
    compileProgramWithTextureCounts("tex", mMaxVertexTextures + 1, mMaxVertexTextures + 1, "tex", 0,
                                    0);
    ASSERT_EQ(0u, mProgram);
}

// Negative test for exceeding the number of fragment textures
TEST_P(TextureLimitsTest, ExcessiveFragmentTextures)
{
    compileProgramWithTextureCounts("tex", 0, 0, "tex", mMaxFragmentTextures + 1,
                                    mMaxFragmentTextures + 1);
    ASSERT_EQ(0u, mProgram);
}

// Test active vertex textures under the limit, but excessive textures specified.
TEST_P(TextureLimitsTest, MaxActiveVertexTextures)
{
    compileProgramWithTextureCounts("tex", mMaxVertexTextures + 4, mMaxVertexTextures, "tex", 0, 0);
    ASSERT_NE(0u, mProgram);
    ASSERT_GL_NO_ERROR();

    testWithTextures(mMaxVertexTextures, "tex", 0, "tex");
}

// Test active fragment textures under the limit, but excessive textures specified.
TEST_P(TextureLimitsTest, MaxActiveFragmentTextures)
{
    compileProgramWithTextureCounts("tex", 0, 0, "tex", mMaxFragmentTextures + 4,
                                    mMaxFragmentTextures);
    ASSERT_NE(0u, mProgram);
    ASSERT_GL_NO_ERROR();

    testWithTextures(0, "tex", mMaxFragmentTextures, "tex");
}

// Negative test for pointing two sampler uniforms of different types to the same texture.
// GLES 2.0.25 section 2.10.4 page 39.
TEST_P(TextureLimitsTest, TextureTypeConflict)
{
    constexpr char kVS[] =
        "attribute vec2 position;\n"
        "varying float color;\n"
        "uniform sampler2D tex2D;\n"
        "uniform samplerCube texCube;\n"
        "void main() {\n"
        "  gl_Position = vec4(position, 0, 1);\n"
        "  vec2 texCoord = (position * 0.5) + 0.5;\n"
        "  color = texture2D(tex2D, texCoord).x;\n"
        "  color += textureCube(texCube, vec3(texCoord, 0)).x;\n"
        "}";
    constexpr char kFS[] =
        "varying mediump float color;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(color, 0, 0, 1);\n"
        "}";

    mProgram = CompileProgram(kVS, kFS);
    ASSERT_NE(0u, mProgram);

    initTextures(1, 0);

    glUseProgram(mProgram);
    GLint tex2DLocation = glGetUniformLocation(mProgram, "tex2D");
    ASSERT_NE(-1, tex2DLocation);
    GLint texCubeLocation = glGetUniformLocation(mProgram, "texCube");
    ASSERT_NE(-1, texCubeLocation);

    glUniform1i(tex2DLocation, 0);
    glUniform1i(texCubeLocation, 0);
    ASSERT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

class Texture2DNorm16TestES3 : public Texture2DTestES3
{
  protected:
    Texture2DNorm16TestES3() : Texture2DTestES3(), mTextures{0, 0, 0}, mFBO(0), mRenderbuffer(0) {}

    void testSetUp() override
    {
        Texture2DTestES3::testSetUp();

        glActiveTexture(GL_TEXTURE0);
        glGenTextures(3, mTextures);
        glGenFramebuffers(1, &mFBO);
        glGenRenderbuffers(1, &mRenderbuffer);

        for (size_t textureIndex = 0; textureIndex < 3; textureIndex++)
        {
            glBindTexture(GL_TEXTURE_2D, mTextures[textureIndex]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        glBindTexture(GL_TEXTURE_2D, 0);

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteTextures(3, mTextures);
        glDeleteFramebuffers(1, &mFBO);
        glDeleteRenderbuffers(1, &mRenderbuffer);

        Texture2DTestES3::testTearDown();
    }

    void testNorm16Texture(GLint internalformat, GLenum format, GLenum type)
    {
        // TODO(http://anglebug.com/4089) Fails on Win Intel OpenGL driver
        ANGLE_SKIP_TEST_IF(IsIntel() && IsOpenGL());
        ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_norm16"));

        GLushort pixelValue  = (type == GL_SHORT) ? 0x7FFF : 0x6A35;
        GLushort imageData[] = {pixelValue, pixelValue, pixelValue, pixelValue};

        setUpProgram();

        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0],
                               0);

        glBindTexture(GL_TEXTURE_2D, mTextures[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16_EXT, 1, 1, 0, GL_RGBA, GL_UNSIGNED_SHORT, nullptr);

        glBindTexture(GL_TEXTURE_2D, mTextures[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, 1, 1, 0, format, type, imageData);

        EXPECT_GL_NO_ERROR();

        drawQuad(mProgram, "position", 0.5f);

        GLubyte expectedValue = (type == GL_SHORT) ? 0xFF : static_cast<GLubyte>(pixelValue >> 8);

        EXPECT_PIXEL_COLOR_EQ(0, 0,
                              SliceFormatColor(format, GLColor(expectedValue, expectedValue,
                                                               expectedValue, expectedValue)));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        ASSERT_GL_NO_ERROR();
    }

    void testReadPixelsRGBAWithRangeAndPixelStoreMode(GLuint x,
                                                      GLuint y,
                                                      GLuint width,
                                                      GLuint height,
                                                      GLint packRowLength,
                                                      GLint packAlignment,
                                                      GLint packSkipPixels,
                                                      GLint packSkipRows,
                                                      GLenum type,
                                                      GLColor16UI color)
    {
        // PACK modes debugging
        GLint s = 2;  // single component size in bytes, UNSIGNED_SHORT -> 2 in our case
        GLint n = 4;  // 4 components per pixel, stands for GL_RGBA

        GLuint l       = packRowLength == 0 ? width : packRowLength;
        const GLint &a = packAlignment;

        // According to
        // https://www.khronos.org/registry/OpenGL-Refpages/es3.0/html/glPixelStorei.xhtml
        GLint k                    = (s >= a) ? n * l : a / s * (1 + (s * n * l - 1) / a);
        std::size_t componentCount = n * packSkipPixels + k * (packSkipRows + height);
        if (static_cast<GLuint>(packRowLength) < width)
        {
            componentCount += width * n * s - k;
        }

        // Populate the pixels array with random dirty value
        constexpr GLushort kDirtyValue = 0x1234;
        std::vector<GLushort> pixels(componentCount, kDirtyValue);
        glReadPixels(x, y, width, height, GL_RGBA, type, pixels.data());

        EXPECT_GL_NO_ERROR();

        GLushort *pixelRowStart = pixels.data();
        pixelRowStart += n * packSkipPixels + k * packSkipRows;

        std::vector<bool> modifiedPixels(componentCount, false);

        char errorInfo[200];

        for (GLuint row = 0; row < height; ++row)
        {
            GLushort *curPixel = pixelRowStart;
            for (GLuint col = 0, len = (row == height - 1) ? width : std::min(l, width); col < len;
                 ++col)
            {
                snprintf(errorInfo, sizeof(errorInfo),
                         "extent: {%u, %u}, coord: (%u, %u), rowLength: %d, alignment: %d, "
                         "skipPixels: %d, skipRows: %d\n",
                         width, height, col, row, packRowLength, packAlignment, packSkipPixels,
                         packSkipRows);
                EXPECT_EQ(color.R, curPixel[0]) << errorInfo;
                EXPECT_EQ(color.G, curPixel[1]) << errorInfo;
                EXPECT_EQ(color.B, curPixel[2]) << errorInfo;
                EXPECT_EQ(color.A, curPixel[3]) << errorInfo;

                std::ptrdiff_t diff      = curPixel - pixels.data();
                modifiedPixels[diff + 0] = true;
                modifiedPixels[diff + 1] = true;
                modifiedPixels[diff + 2] = true;
                modifiedPixels[diff + 3] = true;

                curPixel += n;
            }
            pixelRowStart += k;
        }

        for (std::size_t i = 0; i < modifiedPixels.size(); ++i)
        {
            if (!modifiedPixels[i])
            {
                EXPECT_EQ(pixels[i], kDirtyValue);
            }
        }
    }

    void testNorm16RenderAndReadPixels(GLint internalformat, GLenum format, GLenum type)
    {
        // TODO(http://anglebug.com/4089) Fails on Win Intel OpenGL driver
        ANGLE_SKIP_TEST_IF(IsIntel() && IsOpenGL());
        // TODO(http://anglebug.com/4245) Fails on Win AMD OpenGL driver
        ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && IsDesktopOpenGL());
        ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_norm16"));

        GLushort pixelValue  = 0x6A35;
        GLushort imageData[] = {pixelValue, pixelValue, pixelValue, pixelValue};
        GLColor16UI color    = SliceFormatColor16UI(
               format, GLColor16UI(pixelValue, pixelValue, pixelValue, pixelValue));
        // Size of drawing viewport
        constexpr GLint width = 8, height = 8;

        setUpProgram();

        glBindTexture(GL_TEXTURE_2D, mTextures[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, type, nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1],
                               0);

        glBindTexture(GL_TEXTURE_2D, mTextures[2]);
        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, 1, 1, 0, format, type, imageData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        EXPECT_GL_NO_ERROR();

        drawQuad(mProgram, "position", 0.5f);

        // ReadPixels against different width, height, pixel pack mode combinations to test
        // workaround of pixels rearrangement

        // {x, y, width, height}
        std::vector<std::array<GLint, 4>> areas = {
            {0, 0, 1, 1}, {0, 0, 1, 2}, {0, 0, 2, 1}, {0, 0, 2, 2},
            {0, 0, 3, 2}, {0, 0, 3, 3}, {0, 0, 4, 3}, {0, 0, 4, 4},

            {1, 3, 3, 2}, {1, 3, 3, 3}, {3, 2, 4, 3}, {3, 2, 4, 4},

            {0, 0, 5, 6}, {2, 1, 5, 6}, {0, 0, 6, 1}, {0, 0, 7, 1},
            {0, 0, 7, 3}, {0, 0, 7, 8}, {1, 0, 7, 8}, {0, 0, 8, 8},
        };

        // Put default settings at the last
        std::vector<GLint> paramsPackRowLength = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0};
        std::vector<GLint> paramsPackAlignment = {1, 2, 8, 4};
        std::vector<std::array<GLint, 2>> paramsPackSkipPixelsAndRows = {{1, 0}, {0, 1},   {1, 1},
                                                                         {3, 1}, {20, 20}, {0, 0}};

        // Restore pixel pack modes later
        GLint restorePackAlignment;
        glGetIntegerv(GL_PACK_ALIGNMENT, &restorePackAlignment);
        GLint restorePackRowLength;
        glGetIntegerv(GL_PACK_ROW_LENGTH, &restorePackRowLength);
        GLint restorePackSkipPixels;
        glGetIntegerv(GL_PACK_SKIP_PIXELS, &restorePackSkipPixels);
        GLint restorePackSkipRows;
        glGetIntegerv(GL_PACK_SKIP_ROWS, &restorePackSkipRows);

        // Variable symbols are based on:
        // https://www.khronos.org/registry/OpenGL-Refpages/es3.0/html/glPixelStorei.xhtml
        for (const auto &skipped : paramsPackSkipPixelsAndRows)
        {
            glPixelStorei(GL_PACK_SKIP_PIXELS, skipped[0]);
            glPixelStorei(GL_PACK_SKIP_ROWS, skipped[1]);
            for (GLint a : paramsPackAlignment)
            {
                glPixelStorei(GL_PACK_ALIGNMENT, a);
                for (GLint l : paramsPackRowLength)
                {
                    glPixelStorei(GL_PACK_ROW_LENGTH, l);

                    for (const auto &area : areas)
                    {
                        ASSERT(area[0] + area[2] <= width);
                        ASSERT(area[1] + area[3] <= height);
                        testReadPixelsRGBAWithRangeAndPixelStoreMode(area[0], area[1], area[2],
                                                                     area[3], l, a, skipped[0],
                                                                     skipped[1], type, color);
                    }
                }
            }
        }

        glPixelStorei(GL_PACK_ALIGNMENT, restorePackAlignment);
        glPixelStorei(GL_PACK_ROW_LENGTH, restorePackRowLength);
        glPixelStorei(GL_PACK_SKIP_PIXELS, restorePackSkipPixels);
        glPixelStorei(GL_PACK_SKIP_ROWS, restorePackSkipRows);

        glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, internalformat, 1, 1);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  mRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        EXPECT_GL_NO_ERROR();

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        EXPECT_PIXEL_16UI_COLOR(
            0, 0, SliceFormatColor16UI(format, GLColor16UI(0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF)));

        glBindTexture(GL_TEXTURE_2D, mTextures[1]);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1],
                               0);
        EXPECT_PIXEL_16UI_COLOR(
            0, 0, SliceFormatColor16UI(format, GLColor16UI(0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF)));

        ASSERT_GL_NO_ERROR();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    GLuint mTextures[3];
    GLuint mFBO;
    GLuint mRenderbuffer;
};

TEST_P(Texture2DNorm16TestES3, TextureNorm16R16TextureTest)
{
    testNorm16Texture(GL_R16_EXT, GL_RED, GL_UNSIGNED_SHORT);
}

TEST_P(Texture2DNorm16TestES3, TextureNorm16R16SNORMTextureTest)
{
    testNorm16Texture(GL_R16_SNORM_EXT, GL_RED, GL_SHORT);
}

TEST_P(Texture2DNorm16TestES3, TextureNorm16RG16TextureTest)
{
    testNorm16Texture(GL_RG16_EXT, GL_RG, GL_UNSIGNED_SHORT);
}

TEST_P(Texture2DNorm16TestES3, TextureNorm16RG16SNORMTextureTest)
{
    testNorm16Texture(GL_RG16_SNORM_EXT, GL_RG, GL_SHORT);
}

TEST_P(Texture2DNorm16TestES3, TextureNorm16RGB16TextureTest)
{
    // (http://anglebug.com/4215) Driver bug on some Qualcomm Adreno gpu
    ANGLE_SKIP_TEST_IF(IsNexus5X() && IsOpenGLES());

    testNorm16Texture(GL_RGB16_EXT, GL_RGB, GL_UNSIGNED_SHORT);
}

TEST_P(Texture2DNorm16TestES3, TextureNorm16RGB16SNORMTextureTest)
{
    // (http://anglebug.com/4215) Driver bug on some Qualcomm Adreno gpu
    ANGLE_SKIP_TEST_IF(IsNexus5X() && IsOpenGLES());

    testNorm16Texture(GL_RGB16_SNORM_EXT, GL_RGB, GL_SHORT);
}

TEST_P(Texture2DNorm16TestES3, TextureNorm16RGBA16TextureTest)
{
    testNorm16Texture(GL_RGBA16_EXT, GL_RGBA, GL_UNSIGNED_SHORT);
}

TEST_P(Texture2DNorm16TestES3, TextureNorm16RGBA16SNORMTextureTest)
{
    testNorm16Texture(GL_RGBA16_SNORM_EXT, GL_RGBA, GL_SHORT);
}

TEST_P(Texture2DNorm16TestES3, TextureNorm16R16RenderTest)
{
    // http://anglebug.com/5153
    ANGLE_SKIP_TEST_IF(IsOSX() && IsOpenGL() && IsNVIDIA());

    testNorm16RenderAndReadPixels(GL_R16_EXT, GL_RED, GL_UNSIGNED_SHORT);
}

TEST_P(Texture2DNorm16TestES3, TextureNorm16RG16RenderTest)
{
    // http://anglebug.com/5153
    ANGLE_SKIP_TEST_IF(IsOSX() && IsOpenGL() && IsNVIDIA());

    testNorm16RenderAndReadPixels(GL_RG16_EXT, GL_RG, GL_UNSIGNED_SHORT);
}

TEST_P(Texture2DNorm16TestES3, TextureNorm16RGBA16RenderTest)
{
    // http://anglebug.com/5153
    ANGLE_SKIP_TEST_IF(IsOSX() && IsOpenGL() && IsNVIDIA());

    testNorm16RenderAndReadPixels(GL_RGBA16_EXT, GL_RGBA, GL_UNSIGNED_SHORT);
}

class Texture2DRGTest : public Texture2DTest
{
  protected:
    Texture2DRGTest()
        : Texture2DTest(), mRenderableTexture(0), mTestTexture(0), mFBO(0), mRenderbuffer(0)
    {}

    void testSetUp() override
    {
        Texture2DTest::testSetUp();

        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &mRenderableTexture);
        glGenTextures(1, &mTestTexture);
        glGenFramebuffers(1, &mFBO);
        glGenRenderbuffers(1, &mRenderbuffer);

        glBindTexture(GL_TEXTURE_2D, mRenderableTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, mTestTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glBindTexture(GL_TEXTURE_2D, 0);

        setUpProgram();
        glUseProgram(mProgram);
        glUniform1i(mTexture2DUniformLocation, 0);

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteTextures(1, &mRenderableTexture);
        glDeleteTextures(1, &mTestTexture);
        glDeleteFramebuffers(1, &mFBO);
        glDeleteRenderbuffers(1, &mRenderbuffer);

        Texture2DTest::testTearDown();
    }

    void setupFormatTextures(GLenum internalformat, GLenum format, GLenum type, GLvoid *imageData)
    {
        glBindTexture(GL_TEXTURE_2D, mRenderableTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               mRenderableTexture, 0);

        glBindTexture(GL_TEXTURE_2D, mTestTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, 1, 1, 0, format, type, imageData);

        EXPECT_GL_NO_ERROR();
    }

    void testRGTexture(GLColor expectedColor)
    {
        drawQuad(mProgram, "position", 0.5f);

        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_NEAR(0, 0, expectedColor, kPixelTolerance);
    }

    void testRGRender(GLenum internalformat, GLenum format)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, internalformat, 1, 1);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  mRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        EXPECT_GL_NO_ERROR();

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);

        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, SliceFormatColor(format, GLColor(255u, 255u, 255u, 255u)));
    }

    GLuint mRenderableTexture;
    GLuint mTestTexture;
    GLuint mFBO;
    GLuint mRenderbuffer;
};

// Test unorm texture formats enabled by the GL_EXT_texture_rg extension.
TEST_P(Texture2DRGTest, TextureRGUNormTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_rg"));
    // This workaround causes a GL error on Windows AMD, which is likely a driver bug.
    // The workaround is not intended to be enabled in this configuration so skip it.
    ANGLE_SKIP_TEST_IF(
        getEGLWindow()->isFeatureEnabled(Feature::EmulateCopyTexImage2DFromRenderbuffers) &&
        IsWindows() && IsAMD());

    GLubyte pixelValue  = 0xab;
    GLubyte imageData[] = {pixelValue, pixelValue};

    setupFormatTextures(GL_RED_EXT, GL_RED_EXT, GL_UNSIGNED_BYTE, imageData);
    testRGTexture(
        SliceFormatColor(GL_RED_EXT, GLColor(pixelValue, pixelValue, pixelValue, pixelValue)));
    testRGRender(GL_R8_EXT, GL_RED_EXT);

    setupFormatTextures(GL_RG_EXT, GL_RG_EXT, GL_UNSIGNED_BYTE, imageData);
    testRGTexture(
        SliceFormatColor(GL_RG_EXT, GLColor(pixelValue, pixelValue, pixelValue, pixelValue)));
    testRGRender(GL_RG8_EXT, GL_RG_EXT);
}

// Test float texture formats enabled by the GL_EXT_texture_rg extension.
TEST_P(Texture2DRGTest, TextureRGFloatTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_rg"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float"));

    GLfloat pixelValue  = 0.54321;
    GLfloat imageData[] = {pixelValue, pixelValue};

    GLubyte expectedValue = static_cast<GLubyte>(pixelValue * 255.0f);
    GLColor expectedColor = GLColor(expectedValue, expectedValue, expectedValue, expectedValue);

    setupFormatTextures(GL_RED_EXT, GL_RED_EXT, GL_FLOAT, imageData);
    testRGTexture(SliceFormatColor(GL_RED_EXT, expectedColor));

    setupFormatTextures(GL_RG_EXT, GL_RG_EXT, GL_FLOAT, imageData);
    testRGTexture(SliceFormatColor(GL_RG_EXT, expectedColor));
}

// Test half-float texture formats enabled by the GL_EXT_texture_rg extension.
TEST_P(Texture2DRGTest, TextureRGHalfFloatTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_rg"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_half_float"));

    GLfloat pixelValueFloat = 0.543f;
    GLhalf pixelValue       = 0x3858;
    GLhalf imageData[]      = {pixelValue, pixelValue};

    GLubyte expectedValue = static_cast<GLubyte>(pixelValueFloat * 255.0f);
    GLColor expectedColor = GLColor(expectedValue, expectedValue, expectedValue, expectedValue);

    setupFormatTextures(GL_RED_EXT, GL_RED_EXT, GL_HALF_FLOAT_OES, imageData);
    testRGTexture(SliceFormatColor(GL_RED_EXT, expectedColor));

    setupFormatTextures(GL_RG_EXT, GL_RG_EXT, GL_HALF_FLOAT_OES, imageData);
    testRGTexture(SliceFormatColor(GL_RG_EXT, expectedColor));
}

class Texture2DFloatTest : public Texture2DTest
{
  protected:
    Texture2DFloatTest()
        : Texture2DTest(), mRenderableTexture(0), mTestTexture(0), mFBO(0), mRenderbuffer(0)
    {}

    void testSetUp() override
    {
        Texture2DTest::testSetUp();

        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &mRenderableTexture);
        glGenTextures(1, &mTestTexture);
        glGenFramebuffers(1, &mFBO);
        glGenRenderbuffers(1, &mRenderbuffer);

        setUpProgram();
        glUseProgram(mProgram);
        glUniform1i(mTexture2DUniformLocation, 0);

        glBindTexture(GL_TEXTURE_2D, mRenderableTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               mRenderableTexture, 0);

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteTextures(1, &mRenderableTexture);
        glDeleteTextures(1, &mTestTexture);
        glDeleteFramebuffers(1, &mFBO);
        glDeleteRenderbuffers(1, &mRenderbuffer);

        Texture2DTest::testTearDown();
    }

    void testFloatTextureSample(GLenum internalFormat, GLenum format, GLenum type)
    {
        constexpr GLfloat imageDataFloat[] = {
            0.2f,
            0.3f,
            0.4f,
            0.5f,
        };
        constexpr GLhalf imageDataHalf[] = {
            0x3266,
            0x34CD,
            0x3666,
            0x3800,
        };
        GLColor expectedValue;
        for (int i = 0; i < 4; i++)
        {
            expectedValue[i] = static_cast<GLubyte>(imageDataFloat[i] * 255.0f);
        }

        const GLvoid *imageData;
        switch (type)
        {
            case GL_FLOAT:
                imageData = imageDataFloat;
                break;
            case GL_HALF_FLOAT:
            case GL_HALF_FLOAT_OES:
                imageData = imageDataHalf;
                break;
            default:
                imageData = nullptr;
        }
        ASSERT(imageData != nullptr);

        glBindTexture(GL_TEXTURE_2D, mTestTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, 1, 1, 0, format, type, imageData);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        drawQuad(mProgram, "position", 0.5f);

        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_NEAR(0, 0, SliceFormatColor(format, expectedValue), kPixelTolerance);
    }

    void testFloatTextureLinear(GLenum internalFormat, GLenum format, GLenum type)
    {
        int numComponents;
        switch (format)
        {
            case GL_RGBA:
                numComponents = 4;
                break;
            case GL_RGB:
                numComponents = 3;
                break;
            case GL_LUMINANCE_ALPHA:
                numComponents = 2;
                break;
            case GL_LUMINANCE:
            case GL_ALPHA:
                numComponents = 1;
                break;
            default:
                numComponents = 0;
        }
        ASSERT(numComponents > 0);

        constexpr GLfloat pixelIntensitiesFloat[] = {0.0f, 1.0f, 0.0f, 1.0f};
        constexpr GLhalf pixelIntensitiesHalf[]   = {0x0000, 0x3C00, 0x0000, 0x3C00};

        GLfloat imageDataFloat[16];
        GLhalf imageDataHalf[16];
        for (int i = 0; i < 4; i++)
        {
            for (int c = 0; c < numComponents; c++)
            {
                imageDataFloat[i * numComponents + c] = pixelIntensitiesFloat[i];
                imageDataHalf[i * numComponents + c]  = pixelIntensitiesHalf[i];
            }
        }

        const GLvoid *imageData;
        switch (type)
        {
            case GL_FLOAT:
                imageData = imageDataFloat;
                break;
            case GL_HALF_FLOAT:
            case GL_HALF_FLOAT_OES:
                imageData = imageDataHalf;
                break;
            default:
                imageData = nullptr;
        }
        ASSERT(imageData != nullptr);

        glBindTexture(GL_TEXTURE_2D, mTestTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, 2, 2, 0, format, type, imageData);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        drawQuad(mProgram, "position", 0.5f);

        EXPECT_GL_NO_ERROR();
        // Source texture contains 2 black pixels and 2 white pixels, we sample in the center so we
        // should expect the final value to be gray (halfway in-between)
        EXPECT_PIXEL_COLOR_NEAR(0, 0, SliceFormatColor(format, GLColor(127u, 127u, 127u, 127u)),
                                kPixelTolerance);
    }

    bool performFloatTextureRender(GLenum internalFormat,
                                   GLenum renderBufferFormat,
                                   GLenum format,
                                   GLenum type)
    {
        glBindTexture(GL_TEXTURE_2D, mTestTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, 1, 1, 0, format, type, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, renderBufferFormat, 1, 1);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  mRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        EXPECT_GL_NO_ERROR();

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            return false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        EXPECT_GL_NO_ERROR();
        return true;
    }

    GLuint mRenderableTexture;
    GLuint mTestTexture;
    GLuint mFBO;
    GLuint mRenderbuffer;
};

class Texture2DFloatTestES3 : public Texture2DFloatTest
{
  protected:
    void testFloatTextureRender(GLenum internalFormat, GLenum format, GLenum type)
    {
        bool framebufferComplete =
            performFloatTextureRender(internalFormat, internalFormat, format, type);
        EXPECT_TRUE(framebufferComplete);
        EXPECT_PIXEL_COLOR32F_NEAR(0, 0,
                                   SliceFormatColor32F(format, GLColor32F(1.0f, 1.0f, 1.0f, 1.0f)),
                                   kPixelTolerance32F);
    }
};

class Texture2DFloatTestES2 : public Texture2DFloatTest
{
  protected:
    bool checkFloatTextureRender(GLenum renderBufferFormat, GLenum format, GLenum type)
    {
        bool framebufferComplete =
            performFloatTextureRender(format, renderBufferFormat, format, type);

        if (!framebufferComplete)
        {
            return false;
        }

        EXPECT_PIXEL_COLOR32F_NEAR(0, 0,
                                   SliceFormatColor32F(format, GLColor32F(1.0f, 1.0f, 1.0f, 1.0f)),
                                   kPixelTolerance32F);
        return true;
    }
};

// Test texture sampling for ES3 float texture formats
TEST_P(Texture2DFloatTestES3, TextureFloatSampleBasicTest)
{
    testFloatTextureSample(GL_RGBA32F, GL_RGBA, GL_FLOAT);
    testFloatTextureSample(GL_RGB32F, GL_RGB, GL_FLOAT);
}

// Test texture sampling for ES2 float texture formats
TEST_P(Texture2DFloatTestES2, TextureFloatSampleBasicTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float"));
    testFloatTextureSample(GL_RGBA, GL_RGBA, GL_FLOAT);
    testFloatTextureSample(GL_RGB, GL_RGB, GL_FLOAT);
}

// Test texture sampling for ES3 half float texture formats
TEST_P(Texture2DFloatTestES3, TextureHalfFloatSampleBasicTest)
{
    testFloatTextureSample(GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    testFloatTextureSample(GL_RGB16F, GL_RGB, GL_HALF_FLOAT);
}

// Test texture sampling for ES2 half float texture formats
TEST_P(Texture2DFloatTestES2, TextureHalfFloatSampleBasicTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_half_float"));
    testFloatTextureSample(GL_RGBA, GL_RGBA, GL_HALF_FLOAT_OES);
    testFloatTextureSample(GL_RGB, GL_RGB, GL_HALF_FLOAT_OES);
}

// Test texture sampling for legacy GLES 2.0 float texture formats in ES3
TEST_P(Texture2DFloatTestES3, TextureFloatSampleLegacyTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float"));

    testFloatTextureSample(GL_LUMINANCE, GL_LUMINANCE, GL_FLOAT);
    testFloatTextureSample(GL_ALPHA, GL_ALPHA, GL_FLOAT);
    testFloatTextureSample(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_FLOAT);

    if (IsGLExtensionEnabled("GL_EXT_texture_storage"))
    {
        testFloatTextureSample(GL_LUMINANCE32F_EXT, GL_LUMINANCE, GL_FLOAT);
        testFloatTextureSample(GL_ALPHA32F_EXT, GL_ALPHA, GL_FLOAT);
        testFloatTextureSample(GL_LUMINANCE_ALPHA32F_EXT, GL_LUMINANCE_ALPHA, GL_FLOAT);
    }
}

// Test texture sampling for legacy GLES 2.0 float texture formats in ES2
TEST_P(Texture2DFloatTestES2, TextureFloatSampleLegacyTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float"));

    testFloatTextureSample(GL_LUMINANCE, GL_LUMINANCE, GL_FLOAT);
    testFloatTextureSample(GL_ALPHA, GL_ALPHA, GL_FLOAT);
    testFloatTextureSample(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_FLOAT);
}

// Test texture sampling for legacy GLES 2.0 half float texture formats in ES3
TEST_P(Texture2DFloatTestES3, TextureHalfFloatSampleLegacyTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_half_float"));

    testFloatTextureSample(GL_LUMINANCE, GL_LUMINANCE, GL_HALF_FLOAT_OES);
    testFloatTextureSample(GL_ALPHA, GL_ALPHA, GL_HALF_FLOAT_OES);
    testFloatTextureSample(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES);

    if (IsGLExtensionEnabled("GL_EXT_texture_storage"))
    {
        testFloatTextureSample(GL_LUMINANCE16F_EXT, GL_LUMINANCE, GL_HALF_FLOAT);
        testFloatTextureSample(GL_ALPHA16F_EXT, GL_ALPHA, GL_HALF_FLOAT);
        testFloatTextureSample(GL_LUMINANCE_ALPHA16F_EXT, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT);
    }
}
// Test texture sampling for legacy GLES 2.0 half float texture formats in ES2
TEST_P(Texture2DFloatTestES2, TextureHalfFloatSampleLegacyTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_half_float"));

    testFloatTextureSample(GL_LUMINANCE, GL_LUMINANCE, GL_HALF_FLOAT_OES);
    testFloatTextureSample(GL_ALPHA, GL_ALPHA, GL_HALF_FLOAT_OES);
    testFloatTextureSample(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES);
}

// Test linear sampling for ES3 32F formats
TEST_P(Texture2DFloatTestES3, TextureFloatLinearTest)
{
    // TODO(anglebug.com/5360): Failing on ARM-based Apple DTKs.
    ANGLE_SKIP_TEST_IF(IsOSX() && IsARM64() && (IsDesktopOpenGL()));

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float_linear"));

    testFloatTextureLinear(GL_RGBA32F, GL_RGBA, GL_FLOAT);
    testFloatTextureLinear(GL_RGB32F, GL_RGB, GL_FLOAT);
}
// Test linear sampling for ES2 32F formats
TEST_P(Texture2DFloatTestES2, TextureFloatLinearTest)
{
    // TODO(anglebug.com/5360): Failing on ARM-based Apple DTKs.
    ANGLE_SKIP_TEST_IF(IsOSX() && IsARM64() && (IsDesktopOpenGL()));

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float_linear"));

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float"));

    testFloatTextureLinear(GL_RGBA, GL_RGBA, GL_FLOAT);
}

// Test linear sampling for ES3 16F formats
TEST_P(Texture2DFloatTestES3, TextureHalfFloatLinearTest)
{
    // Half float formats must be linearly filterable in GLES 3.0 core
    testFloatTextureLinear(GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    testFloatTextureLinear(GL_RGB16F, GL_RGB, GL_HALF_FLOAT);
}
// Test linear sampling for ES2 16F formats
TEST_P(Texture2DFloatTestES2, TextureHalfFloatLinearTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_half_float_linear"));
    testFloatTextureLinear(GL_RGBA, GL_RGBA, GL_HALF_FLOAT_OES);
    testFloatTextureLinear(GL_RGB, GL_RGB, GL_HALF_FLOAT_OES);
}

// Test linear sampling for legacy GLES 2.0 32F formats in ES3
TEST_P(Texture2DFloatTestES3, TextureFloatLinearLegacyTest)
{
    // TODO(anglebug.com/5360): Failing on ARM-based Apple DTKs.
    ANGLE_SKIP_TEST_IF(IsOSX() && IsARM64() && (IsDesktopOpenGL()));

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float_linear"));

    testFloatTextureLinear(GL_LUMINANCE, GL_LUMINANCE, GL_FLOAT);
    testFloatTextureLinear(GL_ALPHA, GL_ALPHA, GL_FLOAT);
    testFloatTextureLinear(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_FLOAT);

    if (IsGLExtensionEnabled("GL_EXT_texture_storage"))
    {
        testFloatTextureLinear(GL_LUMINANCE32F_EXT, GL_LUMINANCE, GL_FLOAT);
        testFloatTextureLinear(GL_ALPHA32F_EXT, GL_ALPHA, GL_FLOAT);
        testFloatTextureLinear(GL_LUMINANCE_ALPHA32F_EXT, GL_LUMINANCE_ALPHA, GL_FLOAT);
    }
}
// Test linear sampling for legacy GLES 2.0 32F formats in ES2
TEST_P(Texture2DFloatTestES2, TextureFloatLinearLegacyTest)
{
    // TODO(anglebug.com/5360): Failing on ARM-based Apple DTKs.
    ANGLE_SKIP_TEST_IF(IsOSX() && IsARM64() && (IsDesktopOpenGL()));

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float_linear"));

    testFloatTextureLinear(GL_LUMINANCE, GL_LUMINANCE, GL_FLOAT);
    testFloatTextureLinear(GL_ALPHA, GL_ALPHA, GL_FLOAT);
    testFloatTextureLinear(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_FLOAT);
}

// Test linear sampling for legacy GLES 2.0 16F formats in ES3
TEST_P(Texture2DFloatTestES3, TextureHalfFloatLinearLegacyTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_half_float"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_half_float_linear"));

    testFloatTextureLinear(GL_LUMINANCE, GL_LUMINANCE, GL_HALF_FLOAT_OES);
    testFloatTextureLinear(GL_ALPHA, GL_ALPHA, GL_HALF_FLOAT_OES);
    testFloatTextureLinear(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES);

    if (IsGLExtensionEnabled("GL_EXT_texture_storage"))
    {
        testFloatTextureLinear(GL_LUMINANCE16F_EXT, GL_LUMINANCE, GL_HALF_FLOAT);
        testFloatTextureLinear(GL_ALPHA16F_EXT, GL_ALPHA, GL_HALF_FLOAT);
        testFloatTextureLinear(GL_LUMINANCE_ALPHA16F_EXT, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT);
    }
}
// Test linear sampling for legacy GLES 2.0 16F formats in ES2
TEST_P(Texture2DFloatTestES2, TextureHalfFloatLinearLegacyTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_half_float"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_half_float_linear"));

    testFloatTextureLinear(GL_LUMINANCE, GL_LUMINANCE, GL_HALF_FLOAT_OES);
    testFloatTextureLinear(GL_ALPHA, GL_ALPHA, GL_HALF_FLOAT_OES);
    testFloatTextureLinear(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES);
}

// Test color-renderability for ES3 float and half float textures
TEST_P(Texture2DFloatTestES3, TextureFloatRenderTest)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(IsD3D9());
    // EXT_color_buffer_float covers float, half float, and 11-11-10 float formats
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_color_buffer_float"));

    testFloatTextureRender(GL_R32F, GL_RED, GL_FLOAT);
    testFloatTextureRender(GL_RG32F, GL_RG, GL_FLOAT);
    testFloatTextureRender(GL_RGBA32F, GL_RGBA, GL_FLOAT);

    testFloatTextureRender(GL_R16F, GL_RED, GL_HALF_FLOAT);
    testFloatTextureRender(GL_RG16F, GL_RG, GL_HALF_FLOAT);
    testFloatTextureRender(GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);

    testFloatTextureRender(GL_R11F_G11F_B10F, GL_RGB, GL_FLOAT);
}

// Test color-renderability for ES2 half float textures
TEST_P(Texture2DFloatTestES2, TextureFloatRenderTest)
{
    // EXT_color_buffer_half_float requires at least one format to be renderable, but does not
    // require a specific one
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_color_buffer_half_float"));
    // https://crbug.com/1003971
    ANGLE_SKIP_TEST_IF(IsOzone());
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(IsD3D9());

    bool atLeastOneSupported = false;

    if (IsGLExtensionEnabled("GL_OES_texture_half_float") ||
        IsGLExtensionEnabled("GL_OES_texture_half_float"))
    {
        atLeastOneSupported |= checkFloatTextureRender(GL_R16F_EXT, GL_RED_EXT, GL_HALF_FLOAT_OES);
        atLeastOneSupported |= checkFloatTextureRender(GL_RG16F_EXT, GL_RG_EXT, GL_HALF_FLOAT_OES);
    }
    if (IsGLExtensionEnabled("GL_OES_texture_half_float"))
    {
        atLeastOneSupported |= checkFloatTextureRender(GL_RGB16F_EXT, GL_RGB, GL_HALF_FLOAT_OES);

        // If OES_texture_half_float is supported, then RGBA half float textures must be renderable
        bool rgbaSupported = checkFloatTextureRender(GL_RGBA16F_EXT, GL_RGBA, GL_HALF_FLOAT_OES);
        EXPECT_TRUE(rgbaSupported);
        atLeastOneSupported |= rgbaSupported;
    }

    EXPECT_TRUE(atLeastOneSupported);
}

// Test that UNPACK_SKIP_IMAGES doesn't have an effect on 2D texture uploads.
// GLES 3.0.4 section 3.8.3.
TEST_P(Texture2DTestES3, UnpackSkipImages2D)
{
    // Crashes on Nexus 5X due to a driver bug. http://anglebug.com/1429
    ANGLE_SKIP_TEST_IF(IsNexus5X() && IsOpenGLES());

    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ASSERT_GL_NO_ERROR();

    // SKIP_IMAGES should not have an effect on uploading 2D textures
    glPixelStorei(GL_UNPACK_SKIP_IMAGES, 1000);
    ASSERT_GL_NO_ERROR();

    std::vector<GLColor> pixelsGreen(128u * 128u, GLColor::green);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelsGreen.data());
    ASSERT_GL_NO_ERROR();

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE,
                    pixelsGreen.data());
    ASSERT_GL_NO_ERROR();

    glUseProgram(mProgram);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that skip defined in unpack parameters is taken into account when determining whether
// unpacking source extends outside unpack buffer bounds.
TEST_P(Texture2DTestES3, UnpackSkipPixelsOutOfBounds)
{
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ASSERT_GL_NO_ERROR();

    GLBuffer buf;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf.get());
    std::vector<GLColor> pixelsGreen(128u * 128u, GLColor::green);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, pixelsGreen.size() * 4u, pixelsGreen.data(),
                 GL_DYNAMIC_COPY);
    ASSERT_GL_NO_ERROR();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    ASSERT_GL_NO_ERROR();

    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 1);
    ASSERT_GL_NO_ERROR();

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 1);
    ASSERT_GL_NO_ERROR();

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that unpacking rows that overlap in a pixel unpack buffer works as expected.
TEST_P(Texture2DTestES3, UnpackOverlappingRowsFromUnpackBuffer)
{
    ANGLE_SKIP_TEST_IF(IsD3D11());

    // Incorrect rendering results seen on OSX AMD.
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsOSX() && IsAMD());

    const GLuint width            = 8u;
    const GLuint height           = 8u;
    const GLuint unpackRowLength  = 5u;
    const GLuint unpackSkipPixels = 1u;

    setWindowWidth(width);
    setWindowHeight(height);

    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ASSERT_GL_NO_ERROR();

    GLBuffer buf;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf.get());
    std::vector<GLColor> pixelsGreen((height - 1u) * unpackRowLength + width + unpackSkipPixels,
                                     GLColor::green);

    for (GLuint skippedPixel = 0u; skippedPixel < unpackSkipPixels; ++skippedPixel)
    {
        pixelsGreen[skippedPixel] = GLColor(255, 0, 0, 255);
    }

    glBufferData(GL_PIXEL_UNPACK_BUFFER, pixelsGreen.size() * 4u, pixelsGreen.data(),
                 GL_DYNAMIC_COPY);
    ASSERT_GL_NO_ERROR();

    glPixelStorei(GL_UNPACK_ROW_LENGTH, unpackRowLength);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, unpackSkipPixels);
    ASSERT_GL_NO_ERROR();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    ASSERT_GL_NO_ERROR();

    glUseProgram(mProgram);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    GLuint windowPixelCount = getWindowWidth() * getWindowHeight();
    std::vector<GLColor> actual(windowPixelCount, GLColor::black);
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actual.data());
    std::vector<GLColor> expected(windowPixelCount, GLColor::green);
    EXPECT_EQ(expected, actual);
}

template <typename T>
T UNorm(double value)
{
    return static_cast<T>(value * static_cast<double>(std::numeric_limits<T>::max()));
}

// Test rendering a depth texture with mipmaps.
TEST_P(Texture2DTestES3, DepthTexturesWithMipmaps)
{
    // TODO(cwallez) this is failing on Intel Win7 OpenGL.
    // TODO(zmo) this is faling on Win Intel HD 530 Debug.
    // http://anglebug.com/1706
    ANGLE_SKIP_TEST_IF(IsIntel() && IsWindows() && IsOpenGL());

    // Seems to fail on AMD D3D11. Possibly driver bug. http://anglebug.com/3342
    ANGLE_SKIP_TEST_IF(IsAMD() && IsWindows() && IsD3D11());

    // TODO(cnorthrop): Also failing on Vulkan/Windows/AMD. http://anglebug.com/3950
    ANGLE_SKIP_TEST_IF(IsAMD() && IsWindows() && IsVulkan());

    const int size = getWindowWidth();

    auto dim   = [size](int level) { return size >> level; };
    int levels = gl::log2(size);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexStorage2D(GL_TEXTURE_2D, levels, GL_DEPTH_COMPONENT24, size, size);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    ASSERT_GL_NO_ERROR();

    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);

    std::vector<unsigned char> expected;

    for (int level = 0; level < levels; ++level)
    {
        double value = (static_cast<double>(level) / static_cast<double>(levels - 1));
        expected.push_back(UNorm<unsigned char>(value));

        int levelDim = dim(level);

        ASSERT_GT(levelDim, 0);

        std::vector<unsigned int> initData(levelDim * levelDim, UNorm<unsigned int>(value));
        glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, levelDim, levelDim, GL_DEPTH_COMPONENT,
                        GL_UNSIGNED_INT, initData.data());
    }
    ASSERT_GL_NO_ERROR();

    for (int level = 0; level < levels; ++level)
    {
        glViewport(0, 0, dim(level), dim(level));
        drawQuad(mProgram, "position", 0.5f);
        GLColor actual = ReadColor(0, 0);
        EXPECT_NEAR(expected[level], actual.R, 10u);
    }

    ASSERT_GL_NO_ERROR();
}

class Texture2DDepthTest : public Texture2DTest
{
  protected:
    Texture2DDepthTest() : Texture2DTest() {}

    const char *getVertexShaderSource() override
    {
        return "attribute vec4 vPosition;\n"
               "void main() {\n"
               "  gl_Position = vPosition;\n"
               "}\n";
    }

    const char *getFragmentShaderSource() override
    {
        return "precision mediump float;\n"
               "uniform sampler2D ShadowMap;"
               "void main() {\n"
               "  vec4 shadow_value = texture2D(ShadowMap, vec2(0.5, 0.5));"
               "  if (shadow_value.x == shadow_value.z && shadow_value.x != 0.0) {"
               "    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);"
               "  } else {"
               "    gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
               "  }"
               "}\n";
    }

    bool checkTexImageFormatSupport(GLenum format, GLenum internalformat, GLenum type)
    {
        EXPECT_GL_NO_ERROR();

        GLTexture tex;
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, format, 1, 1, 0, format, type, nullptr);

        return (glGetError() == GL_NO_ERROR);
    }

    void testBehavior(bool useSizedComponent)
    {
        int w                 = getWindowWidth();
        int h                 = getWindowHeight();
        GLuint format         = GL_DEPTH_COMPONENT;
        GLuint internalFormat = GL_DEPTH_COMPONENT;

        if (useSizedComponent)
        {
            internalFormat = GL_DEPTH_COMPONENT24;
        }

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        ASSERT_GL_NO_ERROR();

        GLTexture depthTexture;
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        TexCoordDrawTest::setUpProgram();
        GLint shadowMapLocation = glGetUniformLocation(mProgram, "ShadowMap");
        ASSERT_NE(-1, shadowMapLocation);

        GLint positionLocation = glGetAttribLocation(mProgram, "vPosition");
        ASSERT_NE(-1, positionLocation);

        ANGLE_SKIP_TEST_IF(!checkTexImageFormatSupport(format, internalFormat, GL_UNSIGNED_INT));
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, GL_UNSIGNED_INT, nullptr);
        ASSERT_GL_NO_ERROR();

        // try adding a color buffer.
        GLTexture colorTex;
        glBindTexture(GL_TEXTURE_2D, colorTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        ASSERT_GL_NO_ERROR();

        glViewport(0, 0, w, h);
        // Fill depthTexture with 0.75
        glClearDepthf(0.75);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Revert to normal framebuffer to test depth shader
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepthf(0.0f);
        ASSERT_GL_NO_ERROR();

        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        ASSERT_GL_NO_ERROR();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthTexture);

        glUseProgram(mProgram);
        ASSERT_GL_NO_ERROR();

        glUniform1i(shadowMapLocation, 0);

        const GLfloat gTriangleVertices[] = {-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f};

        glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
        ASSERT_GL_NO_ERROR();
        glEnableVertexAttribArray(positionLocation);
        ASSERT_GL_NO_ERROR();
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        ASSERT_GL_NO_ERROR();

        GLuint pixels[1];
        glReadPixels(w / 2, h / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        ASSERT_GL_NO_ERROR();

        // The GLES 3.x spec says that the depth texture sample can be found in the RED component.
        // However, the OES_depth_texture indicates that the depth value is treated as luminance and
        // is in all the color components. Multiple implementations implement a workaround that
        // follows the OES_depth_texture behavior if the internalformat given at glTexImage2D was a
        // unsized format (e.g. DEPTH_COMPONENT) and the GLES 3.x behavior if it was a sized
        // internalformat such as GL_DEPTH_COMPONENT24. The shader will write out a different color
        // depending on if it sees the texture sample in only the RED component.
        if (useSizedComponent)
        {
            ASSERT_NE(pixels[0], 0xff0000ff);
        }
        else
        {
            ASSERT_EQ(pixels[0], 0xff0000ff);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteProgram(mProgram);
    }
};

// Test depth texture compatibility with OES_depth_texture. Uses unsized internal format.
TEST_P(Texture2DDepthTest, DepthTextureES2Compatibility)
{
    ANGLE_SKIP_TEST_IF(IsD3D11());
    ANGLE_SKIP_TEST_IF(IsIntel() && IsD3D9());
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_depth_texture") &&
                       !IsGLExtensionEnabled("GL_OES_depth_texture"));
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(IsOpenGL() || IsOpenGLES());
    ANGLE_SKIP_TEST_IF(IsARM64() && IsWindows() && IsD3D());

    // When the depth texture is specified with unsized internalformat implementations follow
    // OES_depth_texture behavior. Otherwise they follow GLES 3.0 behavior.
    testBehavior(false);
}

// Test depth texture compatibility with GLES3 using sized internalformat.
TEST_P(Texture2DDepthTest, DepthTextureES3Compatibility)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    // http://anglebug.com/5243
    ANGLE_SKIP_TEST_IF(IsMetal() && !IsMetalTextureSwizzleAvailable());

    testBehavior(true);
}

// Tests unpacking into the unsized GL_ALPHA format.
TEST_P(Texture2DTestES3, UnsizedAlphaUnpackBuffer)
{
    // Initialize the texure.
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, getWindowWidth(), getWindowHeight(), 0, GL_ALPHA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    std::vector<GLubyte> bufferData(getWindowWidth() * getWindowHeight(), 127);

    // Pull in the color data from the unpack buffer.
    GLBuffer unpackBuffer;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer.get());
    glBufferData(GL_PIXEL_UNPACK_BUFFER, getWindowWidth() * getWindowHeight(), bufferData.data(),
                 GL_STATIC_DRAW);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, getWindowWidth(), getWindowHeight(), GL_ALPHA,
                    GL_UNSIGNED_BYTE, nullptr);

    // Clear to a weird color to make sure we're drawing something.
    glClearColor(0.5f, 0.8f, 1.0f, 0.2f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw with the alpha texture and verify.
    drawQuad(mProgram, "position", 0.5f);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(0, 0, 0, 0, 0, 127, 1);
}

// Ensure stale unpack data doesn't propagate in D3D11.
TEST_P(Texture2DTestES3, StaleUnpackData)
{
    // Init unpack buffer.
    GLsizei pixelCount = getWindowWidth() * getWindowHeight() / 2;
    std::vector<GLColor> pixels(pixelCount, GLColor::red);

    GLBuffer unpackBuffer;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer.get());
    GLsizei bufferSize = pixelCount * sizeof(GLColor);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, bufferSize, pixels.data(), GL_STATIC_DRAW);

    // Create from unpack buffer.
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, getWindowWidth() / 2, getWindowHeight() / 2, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    drawQuad(mProgram, "position", 0.5f);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Fill unpack with green, recreating buffer.
    pixels.assign(getWindowWidth() * getWindowHeight(), GLColor::green);
    GLsizei size2 = getWindowWidth() * getWindowHeight() * sizeof(GLColor);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, size2, pixels.data(), GL_STATIC_DRAW);

    // Reinit texture with green.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, getWindowWidth() / 2, getWindowHeight() / 2, GL_RGBA,
                    GL_UNSIGNED_BYTE, nullptr);

    drawQuad(mProgram, "position", 0.5f);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Ensure that texture parameters passed as floats that are converted to ints are rounded before
// validating they are less than 0.
TEST_P(Texture2DTestES3, TextureBaseMaxLevelRoundingValidation)
{
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    // Use a negative number that will round to zero when converted to an integer
    // According to the spec(2.3.1 Data Conversion For State - Setting Commands):
    // "Validation of values performed by state-setting commands is performed after conversion,
    // unless specified otherwise for a specific command."
    GLfloat param = -7.30157126e-07f;
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, param);
    EXPECT_GL_NO_ERROR();

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, param);
    EXPECT_GL_NO_ERROR();
}

// This test covers a D3D format redefinition bug for 3D textures. The base level format was not
// being properly checked, and the texture storage of the previous texture format was persisting.
// This would result in an ASSERT in debug and incorrect rendering in release.
// See http://anglebug.com/1609 and WebGL 2 test conformance2/misc/views-with-offsets.html.
TEST_P(Texture3DTestES3, FormatRedefinitionBug)
{
    GLTexture tex;
    glBindTexture(GL_TEXTURE_3D, tex.get());
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex.get(), 0, 0);

    glCheckFramebufferStatus(GL_FRAMEBUFFER);

    std::vector<uint8_t> pixelData(100, 0);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB565, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, nullptr);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                    pixelData.data());

    ASSERT_GL_NO_ERROR();
}

// Test glTexSubImage using PBO to 3D texture that expose the regression bug
// https://issuetracker.google.com/170657065
TEST_P(Texture3DTestES3, TexSubImageWithPBO)
{
    GLTexture tex;

    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    std::vector<uint8_t> pixelData(128 * 128 * 8 * 4, 0x1f);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 128 * 128 * 8 * 4, pixelData.data(), GL_STATIC_DRAW);

    glBindTexture(GL_TEXTURE_3D, tex.get());
    glTexStorage3D(GL_TEXTURE_3D, 8, GL_RGBA8, 128, 128, 8);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 128, 128, 8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexSubImage3D(GL_TEXTURE_3D, 1, 0, 0, 0, 64, 64, 4, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexSubImage3D(GL_TEXTURE_3D, 2, 0, 0, 0, 32, 32, 2, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexSubImage3D(GL_TEXTURE_3D, 3, 0, 0, 0, 16, 16, 1, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexSubImage3D(GL_TEXTURE_3D, 4, 0, 0, 0, 8, 8, 1, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexSubImage3D(GL_TEXTURE_3D, 5, 0, 0, 0, 4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexSubImage3D(GL_TEXTURE_3D, 6, 0, 0, 0, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexSubImage3D(GL_TEXTURE_3D, 7, 0, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 128, 128, 8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexSubImage3D(GL_TEXTURE_3D, 1, 0, 0, 0, 64, 64, 4, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexSubImage3D(GL_TEXTURE_3D, 2, 0, 0, 0, 32, 32, 2, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
}

// Test basic pixel unpack buffer OOB checks when uploading to a 2D or 3D texture
TEST_P(Texture3DTestES3, BasicUnpackBufferOOB)
{
    // 2D tests
    {
        GLTexture tex;
        glBindTexture(GL_TEXTURE_2D, tex.get());

        GLBuffer pbo;
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo.get());

        // Test OOB
        glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(GLColor) * 2 * 2 - 1, nullptr, GL_STATIC_DRAW);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        ASSERT_GL_ERROR(GL_INVALID_OPERATION);

        // Test OOB
        glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(GLColor) * 2 * 2, nullptr, GL_STATIC_DRAW);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        ASSERT_GL_NO_ERROR();
    }

    // 3D tests
    {
        GLTexture tex;
        glBindTexture(GL_TEXTURE_3D, tex.get());

        GLBuffer pbo;
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo.get());

        // Test OOB
        glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(GLColor) * 2 * 2 * 2 - 1, nullptr,
                     GL_STATIC_DRAW);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        ASSERT_GL_ERROR(GL_INVALID_OPERATION);

        // Test OOB
        glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(GLColor) * 2 * 2 * 2, nullptr, GL_STATIC_DRAW);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        ASSERT_GL_NO_ERROR();
    }
}

// Tests behaviour with a single texture and multiple sampler objects.
TEST_P(Texture2DTestES3, SingleTextureMultipleSamplers)
{
    GLint maxTextureUnits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
    ANGLE_SKIP_TEST_IF(maxTextureUnits < 4);

    constexpr int kSize = 16;

    // Make a single-level texture, fill it with red.
    std::vector<GLColor> redColors(kSize * kSize, GLColor::red);
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 redColors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Simple confidence check.
    draw2DTexturedQuad(0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Bind texture to unit 1 with a sampler object making it incomplete.
    GLSampler sampler;
    glBindSampler(0, sampler);
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Make a mipmap texture, fill it with blue.
    std::vector<GLColor> blueColors(kSize * kSize, GLColor::blue);
    GLTexture mipmapTex;
    glBindTexture(GL_TEXTURE_2D, mipmapTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 blueColors.data());
    glGenerateMipmap(GL_TEXTURE_2D);

    // Draw with the sampler, expect blue.
    draw2DTexturedQuad(0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    // Simple multitexturing program.
    constexpr char kVS[] =
        "#version 300 es\n"
        "in vec2 position;\n"
        "out vec2 texCoord;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 0, 1);\n"
        "    texCoord = position * 0.5 + vec2(0.5);\n"
        "}";

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in vec2 texCoord;\n"
        "uniform sampler2D tex1;\n"
        "uniform sampler2D tex2;\n"
        "uniform sampler2D tex3;\n"
        "uniform sampler2D tex4;\n"
        "out vec4 color;\n"
        "void main()\n"
        "{\n"
        "    color = (texture(tex1, texCoord) + texture(tex2, texCoord) \n"
        "          +  texture(tex3, texCoord) + texture(tex4, texCoord)) * 0.25;\n"
        "}";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    std::array<GLint, 4> texLocations = {
        {glGetUniformLocation(program, "tex1"), glGetUniformLocation(program, "tex2"),
         glGetUniformLocation(program, "tex3"), glGetUniformLocation(program, "tex4")}};
    for (GLint location : texLocations)
    {
        ASSERT_NE(-1, location);
    }

    // Init the uniform data.
    glUseProgram(program);
    for (GLint location = 0; location < 4; ++location)
    {
        glUniform1i(texLocations[location], location);
    }

    // Initialize four samplers
    GLSampler samplers[4];

    // 0: non-mipped.
    glBindSampler(0, samplers[0]);
    glSamplerParameteri(samplers[0], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(samplers[0], GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 1: mipped.
    glBindSampler(1, samplers[1]);
    glSamplerParameteri(samplers[1], GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glSamplerParameteri(samplers[1], GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 2: non-mipped.
    glBindSampler(2, samplers[2]);
    glSamplerParameteri(samplers[2], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(samplers[2], GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 3: mipped.
    glBindSampler(3, samplers[3]);
    glSamplerParameteri(samplers[3], GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glSamplerParameteri(samplers[3], GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Bind two blue mipped textures and two single layer textures, should all draw.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mipmapTex);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, mipmapTex);

    ASSERT_GL_NO_ERROR();

    drawQuad(program, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(0, 0, 128, 0, 128, 255, 2);

    // Bind four single layer textures, two should be incomplete.
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, tex);

    drawQuad(program, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(0, 0, 128, 0, 0, 255, 2);
}

// The test is added to cover http://anglebug.com/2153. Cubemap completeness checks used to start
// always at level 0 instead of the base level resulting in an incomplete texture if the faces at
// level 0 are not created. The test creates a cubemap texture, specifies the images only for mip
// level 1 filled with white color, updates the base level to be 1 and renders a quad. The program
// samples the cubemap using a direction vector (1,1,1).
TEST_P(TextureCubeTestES3, SpecifyAndSampleFromBaseLevel1)
{
    // Check http://anglebug.com/2155.
    ANGLE_SKIP_TEST_IF(IsOSX() && IsNVIDIA());

    constexpr char kVS[] =
        R"(#version 300 es
        precision mediump float;
        in vec3 pos;
        void main() {
            gl_Position = vec4(pos, 1.0);
        })";

    constexpr char kFS[] =
        R"(#version 300 es
        precision mediump float;
        out vec4 color;
        uniform samplerCube uTex;
        void main(){
            color = texture(uTex, vec3(1.0));
        })";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    glUniform1i(glGetUniformLocation(program, "uTex"), 0);
    glActiveTexture(GL_TEXTURE0);

    GLTexture cubeTex;
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTex);

    const int kFaceWidth  = 1;
    const int kFaceHeight = 1;
    std::vector<uint32_t> texData(kFaceWidth * kFaceHeight, 0xFFFFFFFF);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 1, GL_RGBA8, kFaceWidth, kFaceHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, texData.data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 1, GL_RGBA8, kFaceWidth, kFaceHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, texData.data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 1, GL_RGBA8, kFaceWidth, kFaceHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, texData.data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 1, GL_RGBA8, kFaceWidth, kFaceHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, texData.data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 1, GL_RGBA8, kFaceWidth, kFaceHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, texData.data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 1, GL_RGBA8, kFaceWidth, kFaceHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, texData.data());
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 1);

    drawQuad(program, "pos", 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, angle::GLColor::white);
}

// Test GL_PIXEL_UNPACK_BUFFER with GL_TEXTURE_CUBE_MAP.
TEST_P(TextureCubeTestES3, CubeMapPixelUnpackBuffer)
{
    // Check http://anglebug.com/2155.
    ANGLE_SKIP_TEST_IF(IsOSX() && IsNVIDIA());

    constexpr char kVS[] =
        R"(#version 300 es
        precision mediump float;
        in vec3 pos;
        void main() {
            gl_Position = vec4(pos, 1.0);
        })";

    constexpr char kFS[] =
        R"(#version 300 es
        precision mediump float;
        out vec4 color;
        uniform samplerCube uTex;
        void main(){
            color = texture(uTex, vec3(1.0));
        })";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    glUniform1i(glGetUniformLocation(program, "uTex"), 0);
    glActiveTexture(GL_TEXTURE0);

    GLTexture cubeTex;
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTex);

    const int kFaceWidth  = 4;
    const int kFaceHeight = 4;

    uint16_t kHalfFloatOne  = 0x3C00;
    uint16_t kHalfFloatZero = 0;

    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGBA16F, kFaceWidth, kFaceHeight);
    struct RGBA16F
    {
        uint16_t R, G, B, A;
    };
    RGBA16F redColor = {kHalfFloatOne, kHalfFloatZero, kHalfFloatZero, kHalfFloatOne};

    std::vector<RGBA16F> pixels(kFaceWidth * kFaceHeight, redColor);
    GLBuffer buffer;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer.get());
    glBufferData(GL_PIXEL_UNPACK_BUFFER, pixels.size() * sizeof(RGBA16F), pixels.data(),
                 GL_DYNAMIC_DRAW);
    EXPECT_GL_NO_ERROR();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (GLenum faceIndex = 0; faceIndex < 6; faceIndex++)
    {
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, 0, 0, 0, kFaceWidth,
                        kFaceHeight, GL_RGBA, GL_HALF_FLOAT, 0);
        EXPECT_GL_NO_ERROR();
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_REPEAT);

    drawQuad(program, "pos", 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, angle::GLColor::red);
}

// Verify that using negative texture base level and max level generates GL_INVALID_VALUE.
TEST_P(Texture2DTestES3, NegativeTextureBaseLevelAndMaxLevel)
{
    GLuint texture = create2DTexture();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, -1);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, -1);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glDeleteTextures(1, &texture);
    EXPECT_GL_NO_ERROR();
}

// Test setting base level after calling generateMipmap on a LUMA texture.
// Covers http://anglebug.com/2498
TEST_P(Texture2DTestES3, GenerateMipmapAndBaseLevelLUMA)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    constexpr const GLsizei kWidth  = 8;
    constexpr const GLsizei kHeight = 8;
    std::array<GLubyte, kWidth * kHeight * 2> whiteData;
    whiteData.fill(255u);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, kWidth, kHeight, 0, GL_LUMINANCE_ALPHA,
                 GL_UNSIGNED_BYTE, whiteData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, angle::GLColor::white);
}

// Incompatible levels with non-mipmap filtering should work.
TEST_P(Texture2DTestES3, IncompatibleMipsButNoMipmapFiltering)
{
    // http://anglebug.com/4782
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsWindows() && (IsAMD() || IsIntel()));

    // http://anglebug.com/4786
    ANGLE_SKIP_TEST_IF(IsOpenGLES() && IsNVIDIAShield());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    constexpr const GLsizei kSize = 8;
    const std::vector<GLColor> kLevel0Data(kSize * kSize, GLColor::blue);
    const std::vector<GLColor> kLevel1Data(kSize * kSize, GLColor::red);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 kLevel0Data.data());
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 kLevel1Data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    EXPECT_GL_NO_ERROR();

    // Draw with base level 0.  The GL_LINEAR filtering ensures the texture's image is not created
    // with mipmap.
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, kLevel0Data[0]);

    // Verify draw with level 1.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, kLevel1Data[0]);

    // Verify draw with level 0 again
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, kLevel0Data[0]);
}

// Enabling mipmap filtering after previously having used the texture without it should work.
TEST_P(Texture2DTestES3, NoMipmapDrawThenMipmapDraw)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    constexpr const GLsizei kSize = 8;
    const std::vector<GLColor> kLevel0Data(kSize * kSize, GLColor::blue);
    const std::vector<GLColor> kLevelOtherData(kSize * kSize, GLColor::red);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 kLevel0Data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    // Draw so the texture's image is allocated.
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, kLevel0Data[0]);

    // Specify the rest of the image
    for (GLint mip = 1; (kSize >> mip) >= 1; ++mip)
    {
        glTexImage2D(GL_TEXTURE_2D, mip, GL_RGBA, kSize >> mip, kSize >> mip, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, kLevelOtherData.data());
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Texture2DLod(), essl3_shaders::fs::Texture2DLod());
    glUseProgram(program);
    GLint textureLoc = glGetUniformLocation(program, essl3_shaders::Texture2DUniform());
    GLint lodLoc     = glGetUniformLocation(program, essl3_shaders::LodUniform());
    ASSERT_NE(-1, textureLoc);
    ASSERT_NE(-1, lodLoc);
    glUniform1i(textureLoc, 0);

    // Verify the mips
    for (GLint mip = 0; (kSize >> mip) >= 1; ++mip)
    {
        glUniform1f(lodLoc, mip);
        drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, (mip == 0 ? kLevel0Data[0] : kLevelOtherData[0]));
    }
}

// Disabling mipmap filtering after previously having used the texture with it should work.
TEST_P(Texture2DTestES3, MipmapDrawThenNoMipmapDraw)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    constexpr const GLsizei kSize = 8;
    const std::vector<GLColor> kLevel0Data(kSize * kSize, GLColor::blue);
    const std::vector<GLColor> kLevelOtherData(kSize * kSize, GLColor::red);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 kLevel0Data.data());
    for (GLint mip = 1; (kSize >> mip) >= 1; ++mip)
    {
        glTexImage2D(GL_TEXTURE_2D, mip, GL_RGBA, kSize >> mip, kSize >> mip, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, kLevelOtherData.data());
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Texture2DLod(), essl3_shaders::fs::Texture2DLod());
    glUseProgram(program);
    GLint textureLoc = glGetUniformLocation(program, essl3_shaders::Texture2DUniform());
    GLint lodLoc     = glGetUniformLocation(program, essl3_shaders::LodUniform());
    ASSERT_NE(-1, textureLoc);
    ASSERT_NE(-1, lodLoc);
    glUniform1i(textureLoc, 0);

    // Verify the mips.
    for (GLint mip = 0; (kSize >> mip) >= 1; ++mip)
    {
        glUniform1f(lodLoc, mip);
        drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, (mip == 0 ? kLevel0Data[0] : kLevelOtherData[0]));
    }

    // Disable mipmapping and verify mips again.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    for (GLint mip = 0; (kSize >> mip) >= 1; ++mip)
    {
        glUniform1f(lodLoc, mip);
        drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, kLevel0Data[0]);
    }
}

// Respecify texture with more mips.
TEST_P(Texture2DTestES3, RespecifyWithMoreMips)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    constexpr const GLsizei kSize = 8;
    const std::vector<GLColor> kLevelEvenData(kSize * kSize, GLColor::blue);
    const std::vector<GLColor> kLevelOddData(kSize * kSize * 4, GLColor::red);

    auto getLevelData = [&](GLint mip) {
        return mip % 2 == 0 ? kLevelEvenData.data() : kLevelOddData.data();
    };

    for (GLint mip = 0; (kSize >> mip) >= 1; ++mip)
    {
        glTexImage2D(GL_TEXTURE_2D, mip, GL_RGBA, kSize >> mip, kSize >> mip, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, getLevelData(mip));
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Texture2DLod(), essl3_shaders::fs::Texture2DLod());
    glUseProgram(program);
    GLint textureLoc = glGetUniformLocation(program, essl3_shaders::Texture2DUniform());
    GLint lodLoc     = glGetUniformLocation(program, essl3_shaders::LodUniform());
    ASSERT_NE(-1, textureLoc);
    ASSERT_NE(-1, lodLoc);
    glUniform1i(textureLoc, 0);

    // Verify the mips.
    for (GLint mip = 0; (kSize >> mip) >= 1; ++mip)
    {
        glUniform1f(lodLoc, mip);
        drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, getLevelData(mip)[0]);
    }

    // Respecify the texture with more mips, without changing any parameters.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize * 2, kSize * 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 kLevelOddData.data());
    for (GLint mip = 0; (kSize >> mip) >= 1; ++mip)
    {
        glTexImage2D(GL_TEXTURE_2D, mip + 1, GL_RGBA, kSize >> mip, kSize >> mip, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, getLevelData(mip));
    }

    // Verify the mips.
    for (GLint mip = 0; ((kSize * 2) >> mip) >= 1; ++mip)
    {
        glUniform1f(lodLoc, mip);
        drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, getLevelData(mip - 1)[0]);
    }
}

// Covers a bug in the D3D11 backend: http://anglebug.com/2772
// When using a sampler the texture was created as if it has mipmaps,
// regardless what you specified in GL_TEXTURE_MIN_FILTER via
// glSamplerParameteri() -- mistakenly the default value
// GL_NEAREST_MIPMAP_LINEAR or the value set via glTexParameteri() was
// evaluated.
// If you didn't provide mipmaps and didn't let the driver generate them
// this led to not sampling your texture data when minification occurred.
TEST_P(Texture2DTestES3, MinificationWithSamplerNoMipmapping)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "out vec2 texcoord;\n"
        "in vec4 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position.xy * 0.1, 0.0, 1.0);\n"
        "    texcoord = (position.xy * 0.5) + 0.5;\n"
        "}\n";

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "uniform highp sampler2D tex;\n"
        "in vec2 texcoord;\n"
        "out vec4 fragColor;\n"
        "void main()\n"
        "{\n"
        "    fragColor = texture(tex, texcoord);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    GLSampler sampler;
    glBindSampler(0, sampler);
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    const GLsizei texWidth  = getWindowWidth();
    const GLsizei texHeight = getWindowHeight();
    const std::vector<GLColor> whiteData(texWidth * texHeight, GLColor::white);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 whiteData.data());
    EXPECT_GL_NO_ERROR();

    drawQuad(program, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, angle::GLColor::white);
}

void Texture2DTest::testUploadThenUseInDifferentStages(
    const std::vector<UploadThenUseStageParam> &uses)
{
    constexpr char kVSSampleVS[] = R"(attribute vec4 a_position;
uniform sampler2D u_tex2D;
varying vec4 v_color;

void main()
{
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
    v_color = texture2D(u_tex2D, a_position.xy * 0.5 + vec2(0.5));
})";

    constexpr char kVSSampleFS[] = R"(precision mediump float;
varying vec4 v_color;

void main()
{
    gl_FragColor = v_color;
})";

    ANGLE_GL_PROGRAM(sampleInVS, kVSSampleVS, kVSSampleFS);
    ANGLE_GL_PROGRAM(sampleInFS, essl1_shaders::vs::Texture2D(), essl1_shaders::fs::Texture2D());

    GLFramebuffer fbo[2];
    GLTexture color[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        glBindTexture(GL_TEXTURE_2D, color[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color[i], 0);
    }

    const GLColor kImageColor(63, 31, 0, 255);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &kImageColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glActiveTexture(GL_TEXTURE0);
    ASSERT_GL_NO_ERROR();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    glClearColor(0, 0, 0, 1);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo[1]);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[0]);
    glClear(GL_COLOR_BUFFER_BIT);

    uint32_t curFboIndex     = 0;
    uint32_t fboDrawCount[2] = {};

    for (const UploadThenUseStageParam &use : uses)
    {
        const GLProgram &program = use.useStage == GL_VERTEX_SHADER ? sampleInVS : sampleInFS;
        drawQuad(program, essl1_shaders::PositionAttrib(), 0.5);
        ASSERT_GL_NO_ERROR();

        ++fboDrawCount[curFboIndex];

        if (use.closeRenderPassAfterUse)
        {
            // Close the render pass without accidentally incurring additional barriers.
            curFboIndex = 1 - curFboIndex;
            glBindFramebuffer(GL_FRAMEBUFFER, fbo[curFboIndex]);
        }
    }

    // Make sure the transfer operations below aren't reordered with the rendering above and thus
    // introduce additional synchronization.
    glFinish();

    for (uint32_t i = 0; i < 2; ++i)
    {
        const GLColor kExpectedColor(63 * std::min(4u, fboDrawCount[i]),
                                     31 * std::min(8u, fboDrawCount[i]), 0, 255);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        EXPECT_PIXEL_COLOR_EQ(0, 0, kExpectedColor);
    }
}

// Test synchronization when a texture is used in different shader stages after data upload.
//
// - Use in VS
// - Use in FS
TEST_P(Texture2DTest, UploadThenVSThenFS)
{
    testUploadThenUseInDifferentStages({
        {GL_VERTEX_SHADER, false},
        {GL_FRAGMENT_SHADER, false},
    });
}

// Test synchronization when a texture is used in different shader stages after data upload.
//
// - Use in VS
// - Break render pass
// - Use in FS
TEST_P(Texture2DTest, UploadThenVSThenNewRPThenFS)
{
    testUploadThenUseInDifferentStages({
        {GL_VERTEX_SHADER, true},
        {GL_FRAGMENT_SHADER, false},
    });
}

// Test synchronization when a texture is used in different shader stages after data upload.
//
// - Use in FS
// - Use in VS
TEST_P(Texture2DTest, UploadThenFSThenVS)
{
    testUploadThenUseInDifferentStages({
        {GL_FRAGMENT_SHADER, false},
        {GL_VERTEX_SHADER, false},
    });
}

// Test synchronization when a texture is used in different shader stages after data upload.
//
// - Use in FS
// - Break render pass
// - Use in VS
TEST_P(Texture2DTest, UploadThenFSThenNewRPThenVS)
{
    testUploadThenUseInDifferentStages({
        {GL_FRAGMENT_SHADER, true},
        {GL_VERTEX_SHADER, false},
    });
}

// Test synchronization when a texture is used in different shader stages after data upload.
//
// - Use in VS
// - Use in FS
// - Use in VS
TEST_P(Texture2DTest, UploadThenVSThenFSThenVS)
{
    testUploadThenUseInDifferentStages({
        {GL_VERTEX_SHADER, false},
        {GL_FRAGMENT_SHADER, false},
        {GL_VERTEX_SHADER, false},
    });
}

// Test synchronization when a texture is used in different shader stages after data upload.
//
// - Use in VS
// - Break render pass
// - Use in FS
// - Use in VS
TEST_P(Texture2DTest, UploadThenVSThenNewRPThenFSThenVS)
{
    testUploadThenUseInDifferentStages({
        {GL_VERTEX_SHADER, true},
        {GL_FRAGMENT_SHADER, false},
        {GL_VERTEX_SHADER, false},
    });
}

// Test synchronization when a texture is used in different shader stages after data upload.
//
// - Use in VS
// - Break render pass
// - Use in FS
// - Break render pass
// - Use in VS
TEST_P(Texture2DTest, UploadThenVSThenNewRPThenFSThenNewRPThenVS)
{
    testUploadThenUseInDifferentStages({
        {GL_VERTEX_SHADER, true},
        {GL_FRAGMENT_SHADER, true},
        {GL_VERTEX_SHADER, false},
    });
}

// Test synchronization when a texture is used in different shader stages after data upload.
//
// - Use in FS
// - Use in VS
// - Break render pass
// - Use in FS
TEST_P(Texture2DTest, UploadThenFSThenVSThenNewRPThenFS)
{
    testUploadThenUseInDifferentStages({
        {GL_FRAGMENT_SHADER, false},
        {GL_VERTEX_SHADER, true},
        {GL_FRAGMENT_SHADER, false},
    });
}

// Test synchronization when a texture is used in different shader stages after data upload.
//
// - Use in FS
// - Break render pass
// - Use in VS
// - Use in FS
TEST_P(Texture2DTest, UploadThenFSThenNewRPThenVSThenFS)
{
    testUploadThenUseInDifferentStages({
        {GL_FRAGMENT_SHADER, true},
        {GL_VERTEX_SHADER, false},
        {GL_FRAGMENT_SHADER, false},
    });
}

// Test synchronization when a texture is used in different shader stages after data upload.
//
// - Use in FS
// - Break render pass
// - Use in FS
// - Use in VS
TEST_P(Texture2DTest, UploadThenFSThenNewRPThenFSThenVS)
{
    testUploadThenUseInDifferentStages({
        {GL_FRAGMENT_SHADER, true},
        {GL_FRAGMENT_SHADER, false},
        {GL_VERTEX_SHADER, false},
    });
}

// Test that clears due to emulated formats are to the correct level given non-zero base level.
TEST_P(Texture2DTestES3, NonZeroBaseEmulatedClear)
{
    // Tests behavior of the Vulkan backend with emulated formats.
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    // This test assumes GL_RGB is always emulated, which overrides the
    // Feature::AllocateNonZeroMemory memory feature, clearing the memory to zero. However, if the
    // format is *not* emulated and the feature Feature::AllocateNonZeroMemory is enabled, the
    // texture memory will contain non-zero memory, which means the color is not black (causing the
    // test to fail).
    ANGLE_SKIP_TEST_IF(getEGLWindow()->isFeatureEnabled(Feature::AllocateNonZeroMemory));

    setUpProgram();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGB, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 3, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 4, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 4);
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
}

// Test that uploading data to buffer that's in use then using it as PBO to update a texture works.
TEST_P(Texture2DTestES3, UseAsUBOThenUpdateThenAsPBO)
{
    const std::array<GLColor, 4> kInitialData = {GLColor::red, GLColor::red, GLColor::red,
                                                 GLColor::red};
    const std::array<GLColor, 4> kUpdateData  = {GLColor::blue, GLColor::blue, GLColor::blue,
                                                 GLColor::blue};

    GLBuffer buffer;
    glBindBuffer(GL_UNIFORM_BUFFER, buffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(kInitialData), kInitialData.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, buffer);
    EXPECT_GL_NO_ERROR();

    constexpr char kVerifyUBO[] = R"(#version 300 es
precision mediump float;
uniform block {
    uvec4 data;
} ubo;
out vec4 colorOut;
void main()
{
    if (all(equal(ubo.data, uvec4(0xFF0000FFu))))
        colorOut = vec4(0, 1.0, 0, 1.0);
    else
        colorOut = vec4(1.0, 0, 0, 1.0);
})";

    ANGLE_GL_PROGRAM(verifyUbo, essl3_shaders::vs::Simple(), kVerifyUBO);
    drawQuad(verifyUbo, essl3_shaders::PositionAttrib(), 0.5);
    EXPECT_GL_NO_ERROR();

    // Update buffer data
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(kInitialData), kUpdateData.data());
    EXPECT_GL_NO_ERROR();

    // Bind as PBO
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
    EXPECT_GL_NO_ERROR();

    // Upload from PBO to texture
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_NO_ERROR();

    // Make sure uniform data is correct.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Make sure the texture data is correct.
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    EXPECT_GL_NO_ERROR();
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
}

// Test if the RenderTargetCache is updated when the TextureStorage object is freed
TEST_P(Texture2DTestES3, UpdateRenderTargetCacheOnDestroyTexStorage)
{
    ANGLE_GL_PROGRAM(drawRed, essl3_shaders::vs::Simple(), essl3_shaders::fs::Red());
    const GLenum attachments[] = {GL_COLOR_ATTACHMENT0};

    GLTexture tex;
    GLFramebuffer fb;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 2, GL_RGBA8, 100, 1);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, attachments);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    drawQuad(drawRed, essl3_shaders::PositionAttrib(), 1.0f);
    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_RECT_EQ(0, 0, 100, 1, GLColor::red);
}

// Draw a quad with an integer texture with a non-zero base level, and test that the color of the
// texture is output.
TEST_P(Texture2DIntegerTestES3, IntegerTextureNonZeroBaseLevel)
{
    // http://anglebug.com/3478
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && IsDesktopOpenGL());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    int width     = getWindowWidth();
    int height    = getWindowHeight();
    GLColor color = GLColor::green;
    std::vector<GLColor> pixels(width * height, color);
    GLint baseLevel = 1;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, baseLevel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, baseLevel, GL_RGBA8UI, width, height, 0, GL_RGBA_INTEGER,
                 GL_UNSIGNED_BYTE, pixels.data());

    setUpProgram();
    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, color);
    EXPECT_PIXEL_COLOR_EQ(width - 1, height - 1, color);
}

// Draw a quad with an integer cube texture with a non-zero base level, and test that the color of
// the texture is output.
TEST_P(TextureCubeIntegerTestES3, IntegerCubeTextureNonZeroBaseLevel)
{
    // All output checks returned black, rather than the texture color.
    ANGLE_SKIP_TEST_IF(IsOSX() && IsOpenGL());

    glActiveTexture(GL_TEXTURE0);

    glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureCube);
    GLint baseLevel = 1;
    int width       = getWindowWidth();
    int height      = getWindowHeight();
    GLColor color   = GLColor::green;
    std::vector<GLColor> pixels(width * height, color);
    for (GLenum faceIndex = 0; faceIndex < 6; faceIndex++)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, baseLevel, GL_RGBA8UI, width,
                     height, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, pixels.data());
        EXPECT_GL_NO_ERROR();
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, baseLevel);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glUseProgram(mProgram);
    glUniform1i(mTextureCubeUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, color);
    EXPECT_PIXEL_COLOR_EQ(width - 1, 0, color);
    EXPECT_PIXEL_COLOR_EQ(0, height - 1, color);
    EXPECT_PIXEL_COLOR_EQ(width - 1, height - 1, color);
}

// This test sets up a cube map with four distincly colored MIP levels.
// The size of the texture and the geometry is chosen such that levels 1 or 2 should be chosen at
// the corners of the screen.
TEST_P(TextureCubeIntegerEdgeTestES3, IntegerCubeTextureCorner)
{
    glActiveTexture(GL_TEXTURE0);

    glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureCube);
    int width  = getWindowWidth();
    int height = getWindowHeight();
    ASSERT_EQ(width, height);
    GLColor color[4] = {GLColor::white, GLColor::green, GLColor::blue, GLColor::red};
    for (GLint level = 0; level < 4; level++)
    {
        for (GLenum faceIndex = 0; faceIndex < 6; faceIndex++)
        {
            int levelWidth  = (2 * width) >> level;
            int levelHeight = (2 * height) >> level;
            std::vector<GLColor> pixels(levelWidth * levelHeight, color[level]);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, level, GL_RGBA8UI, levelWidth,
                         levelHeight, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, pixels.data());
            EXPECT_GL_NO_ERROR();
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 3);

    glUseProgram(mProgram);
    glUniform1i(mTextureCubeUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);

    ASSERT_GL_NO_ERROR();
    // Check that we do not read from levels 0 or 3. Levels 1 and 2 are both acceptable.
    EXPECT_EQ(ReadColor(0, 0).R, 0);
    EXPECT_EQ(ReadColor(width - 1, 0).R, 0);
    EXPECT_EQ(ReadColor(0, height - 1).R, 0);
    EXPECT_EQ(ReadColor(width - 1, height - 1).R, 0);
}

// Draw a quad with an integer texture with a non-zero base level, and test that the color of the
// texture is output.
TEST_P(Texture2DIntegerProjectiveOffsetTestES3, NonZeroBaseLevel)
{
    // Fails on AMD: http://crbug.com/967796
    ANGLE_SKIP_TEST_IF(IsAMD() && IsWindows() && IsOpenGL());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    int width     = getWindowWidth();
    int height    = getWindowHeight();
    GLColor color = GLColor::green;
    std::vector<GLColor> pixels(width * height, color);
    GLint baseLevel = 1;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, baseLevel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, baseLevel, GL_RGBA8UI, width, height, 0, GL_RGBA_INTEGER,
                 GL_UNSIGNED_BYTE, pixels.data());

    setUpProgram();
    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    drawQuad(mProgram, "position", 0.5f);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, color);
    EXPECT_PIXEL_COLOR_EQ(width - 1, height - 1, color);
}

// Draw a quad with an integer texture with a non-zero base level, and test that the color of the
// texture is output.
TEST_P(Texture2DArrayIntegerTestES3, NonZeroBaseLevel)
{
    // Test fail: http://anglebug.com/5959
    ANGLE_SKIP_TEST_IF(IsIntel() && IsOSX() && IsOpenGL());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m2DArrayTexture);
    int width     = getWindowWidth();
    int height    = getWindowHeight();
    int depth     = 2;
    GLColor color = GLColor::green;
    std::vector<GLColor> pixels(width * height * depth, color);
    GLint baseLevel = 1;
    glTexImage3D(GL_TEXTURE_2D_ARRAY, baseLevel, GL_RGBA8UI, width, height, depth, 0,
                 GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, baseLevel);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, color);
    EXPECT_PIXEL_COLOR_EQ(width - 1, height - 1, color);
}

// Draw a quad with an integer 3D texture with a non-zero base level, and test that the color of the
// texture is output.
TEST_P(Texture3DIntegerTestES3, NonZeroBaseLevel)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, mTexture3D);
    int width     = getWindowWidth();
    int height    = getWindowHeight();
    int depth     = 2;
    GLColor color = GLColor::green;
    std::vector<GLColor> pixels(width * height * depth, color);
    GLint baseLevel = 1;
    glTexImage3D(GL_TEXTURE_3D, baseLevel, GL_RGBA8UI, width, height, depth, 0, GL_RGBA_INTEGER,
                 GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, baseLevel);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    drawQuad(mProgram, "position", 0.5f);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, color);
    EXPECT_PIXEL_COLOR_EQ(width - 1, height - 1, color);
}

void PBOCompressedTextureTest::runCompressedSubImage()
{
    // ETC texture formats are not supported on Mac OpenGL. http://anglebug.com/3853
    ANGLE_SKIP_TEST_IF(IsOSX() && IsDesktopOpenGL());
    // http://anglebug.com/4115
    ANGLE_SKIP_TEST_IF(IsAMD() && IsWindows() && IsDesktopOpenGL());
    ANGLE_SKIP_TEST_IF(IsIntel() && IsWindows() && IsDesktopOpenGL());

    if (getClientMajorVersion() < 3)
    {
        ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_storage"));
        ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_NV_pixel_buffer_object"));
        ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_compressed_ETC2_RGB8_texture"));
    }

    const GLuint width  = 4u;
    const GLuint height = 4u;

    setWindowWidth(width);
    setWindowHeight(height);

    // Setup primary Texture
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    if (getClientMajorVersion() < 3)
    {
        glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGB8_ETC2, width, height);
    }
    else
    {
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGB8_ETC2, width, height);
    }
    ASSERT_GL_NO_ERROR();

    // Setup PBO and fill it with a red
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mPBO);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height / 2u, kCompressedImageETC2, GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    // Write PBO to mTexture
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_COMPRESSED_RGB8_ETC2,
                              width * height / 2u, nullptr);
    ASSERT_GL_NO_ERROR();

    setUpProgram();
    // Draw using PBO updated texture
    glUseProgram(mProgram);
    glUniform1i(mTexture2DUniformLocation, 0);
    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);
    ASSERT_GL_NO_ERROR();
}

// Test that uses glCompressedTexSubImage2D combined with a PBO
TEST_P(PBOCompressedTextureTest, PBOCompressedSubImage)
{
    runCompressedSubImage();
}

// Verify the row length state is ignored when using compressed tex image calls.
TEST_P(PBOCompressedTextureTest, PBOCompressedSubImageWithUnpackRowLength)
{
    // ROW_LENGTH requires ES3 or an extension.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       !IsGLExtensionEnabled("GL_EXT_unpack_subimage"));

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 1);
    runCompressedSubImage();
}

class PBOCompressedTexture3DTest : public ANGLETest<>
{
  protected:
    PBOCompressedTexture3DTest() {}
};

// Test that uses glCompressedTexSubImage3D combined with a PBO
TEST_P(PBOCompressedTexture3DTest, 2DArray)
{
    // We use GetTexImage to determine if the internal texture format is emulated
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_get_image"));

    const GLuint width  = 4u;
    const GLuint height = 4u;
    const GLuint depth  = 1u;

    setWindowWidth(width);
    setWindowHeight(height);

    // Setup primary texture as a 2DArray holding ETC2 data
    GLTexture texture2DArray;
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture2DArray);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_COMPRESSED_RGB8_ETC2, width, height, depth);

    // If the format emulated, we can't transfer it from a PBO
    ANGLE_SKIP_TEST_IF(IsFormatEmulated(GL_TEXTURE_2D_ARRAY));

    // Set up a VS that simply passes through position and texcord
    const char kVS[] = R"(#version 300 es
in vec4 position;
out vec3 texCoord;

void main()
{
    gl_Position = vec4(position.xy, 0.0, 1.0);
    texCoord = vec3(position.xy * 0.5 + vec2(0.5), 0.0);
})";

    // and FS that pulls from the 2DArray, writing out color
    const char kFS[] = R"(#version 300 es
precision mediump float;
uniform highp sampler2DArray tex2DArray;
in vec3 texCoord;
out vec4 fragColor;

void main()
{
    fragColor = texture(tex2DArray, texCoord);
})";

    // Compile the shaders and create the program
    ANGLE_GL_PROGRAM(program, kVS, kFS);
    ASSERT_GL_NO_ERROR();

    // Setup PBO and fill it with a red
    GLBuffer pbo;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * depth / 2u, kCompressedImageETC2,
                 GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    // Write PBO to texture2DArray
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, width, height, depth,
                              GL_COMPRESSED_RGB8_ETC2, width * height * depth / 2u, nullptr);

    ASSERT_GL_NO_ERROR();

    // Draw using PBO updated texture
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "tex2DArray"), 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture2DArray);
    drawQuad(program, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    // Verify the texture now contains data from the PBO
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);
    ASSERT_GL_NO_ERROR();
}

// Test using ETC1_RGB8 with subimage updates
TEST_P(ETC1CompressedTextureTest, ETC1CompressedSubImage)
{
    // ETC texture formats are not supported on Mac OpenGL. http://anglebug.com/3853
    ANGLE_SKIP_TEST_IF(IsOSX() && IsDesktopOpenGL());

    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       !IsGLExtensionEnabled("GL_EXT_texture_storage"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_compressed_ETC1_RGB8_sub_texture"));

    const GLuint width  = 4u;
    const GLuint height = 4u;

    setWindowWidth(width);
    setWindowHeight(height);

    // Setup primary Texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    if (getClientMajorVersion() < 3)
    {
        glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_ETC1_RGB8_OES, width, height);
    }
    else
    {
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_ETC1_RGB8_OES, width, height);
    }
    ASSERT_GL_NO_ERROR();

    // Populate a subimage of the texture
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_ETC1_RGB8_OES,
                              width * height / 2u, kCompressedImageETC2);
    ASSERT_GL_NO_ERROR();

    // Render and ensure we get red
    glUseProgram(mProgram);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);
    ASSERT_GL_NO_ERROR();
}

// Fully-define a NPOT compressed texture and draw; set MAX_LEVEL and draw; then increase
// MAX_LEVEL and draw.  This used to cause Vulkan validation errors.
TEST_P(ETC1CompressedTextureTest, ETC1CompressedImageNPOT)
{
    // ETC texture formats are not supported on Mac OpenGL. http://anglebug.com/3853
    ANGLE_SKIP_TEST_IF(IsOSX() && IsDesktopOpenGL());

    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_compressed_ETC1_RGB8_sub_texture"));

    const GLuint width  = 5u;
    const GLuint height = 5u;
    // round up to the nearest block size
    const GLsizei imageSize = 8 * 8 / 2;
    // smallest block size
    const GLsizei minImageSize = 4 * 4 / 2;

    uint8_t data[imageSize] = {0};

    setWindowWidth(width);
    setWindowHeight(height);

    // Setup primary Texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_ETC1_RGB8_OES, width, height, 0, imageSize, data);
    ASSERT_GL_NO_ERROR();

    glCompressedTexImage2D(GL_TEXTURE_2D, 1, GL_ETC1_RGB8_OES, width / 2, height / 2, 0,
                           minImageSize, data);
    ASSERT_GL_NO_ERROR();

    glCompressedTexImage2D(GL_TEXTURE_2D, 2, GL_ETC1_RGB8_OES, width / 4, height / 4, 0,
                           minImageSize, data);
    ASSERT_GL_NO_ERROR();

    glUseProgram(mProgram);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
}

// Define two NPOT compressed textures, set MAX_LEVEL, draw, and swap buffers
// with the two textures. This used to cause release of staging buffers
// that have not been flushed.
TEST_P(ETC1CompressedTextureTest, ETC1CompressedImageDraws)
{
    // ETC texture formats are not supported on Mac OpenGL. http://anglebug.com/3853
    ANGLE_SKIP_TEST_IF(IsOSX() && IsDesktopOpenGL());

    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_compressed_ETC1_RGB8_sub_texture"));

    const GLuint width  = 384u;
    const GLuint height = 384u;
    // round up to the nearest block size
    const GLsizei imageSize = width * height / 2;

    uint8_t data[imageSize] = {0};

    setWindowWidth(width);
    setWindowHeight(height);

    const GLuint smallerWidth  = 384u;
    const GLuint smallerHeight = 320u;
    // round up to the nearest block size
    const GLsizei smallerImageSize = smallerWidth * smallerHeight / 2;

    // Setup primary Texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_ETC1_RGB8_OES, smallerWidth, smallerHeight, 0,
                           smallerImageSize, data);
    ASSERT_GL_NO_ERROR();

    glCompressedTexImage2D(GL_TEXTURE_2D, 1, GL_ETC1_RGB8_OES, 192, 160, 0, 15360, data);
    ASSERT_GL_NO_ERROR();

    GLTexture largerTexture;
    glBindTexture(GL_TEXTURE_2D, largerTexture);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_ETC1_RGB8_OES, width, height, 0, imageSize, data);
    ASSERT_GL_NO_ERROR();

    glCompressedTexImage2D(GL_TEXTURE_2D, 1, GL_ETC1_RGB8_OES, 192, 192, 0, 18432, data);
    ASSERT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    glUseProgram(mProgram);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
    swapBuffers();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
    swapBuffers();

    glBindTexture(GL_TEXTURE_2D, largerTexture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
    swapBuffers();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
    swapBuffers();

    glBindTexture(GL_TEXTURE_2D, mTexture2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    drawQuad(mProgram, "position", 0.5f);
    swapBuffers();
    ASSERT_GL_NO_ERROR();
}

// Fully-define a compressed texture and draw; then decrease MAX_LEVEL and draw; then increase
// MAX_LEVEL and draw.  This used to cause Vulkan validation errors.
TEST_P(ETC1CompressedTextureTest, ETC1ShrinkThenGrowMaxLevels)
{
    // ETC texture formats are not supported on Mac OpenGL. http://anglebug.com/3853
    ANGLE_SKIP_TEST_IF(IsOSX() && IsDesktopOpenGL());

    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_compressed_ETC1_RGB8_sub_texture"));

    const GLuint width  = 4u;
    const GLuint height = 4u;

    setWindowWidth(width);
    setWindowHeight(height);

    // Setup primary Texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    if (getClientMajorVersion() < 3)
    {
        glTexStorage2DEXT(GL_TEXTURE_2D, 3, GL_ETC1_RGB8_OES, width, height);
    }
    else
    {
        glTexStorage2D(GL_TEXTURE_2D, 3, GL_ETC1_RGB8_OES, width, height);
    }
    ASSERT_GL_NO_ERROR();

    // Populate a subimage of the texture
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_ETC1_RGB8_OES,
                              width * height / 2u, kCompressedImageETC2);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, width / 2, height / 2, GL_ETC1_RGB8_OES,
                              width * height / 2u, kCompressedImageETC2);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 2, 0, 0, width / 4, height / 4, GL_ETC1_RGB8_OES,
                              width * height / 2u, kCompressedImageETC2);
    ASSERT_GL_NO_ERROR();

    // Set MAX_LEVEL to 2 (the highest level)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2);

    // Render and ensure we get red
    glUseProgram(mProgram);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);
    ASSERT_GL_NO_ERROR();

    // Decrease MAX_LEVEL to 0, render, and ensure we still get red
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);
    ASSERT_GL_NO_ERROR();

    // Increase MAX_LEVEL back to 2, render, and ensure we still get red
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);
    ASSERT_GL_NO_ERROR();
}

class TextureBufferTestES31 : public ANGLETest<>
{
  protected:
    TextureBufferTestES31() {}
};

// Test that mutating a buffer attached to a texture returns correct results in query.
TEST_P(TextureBufferTestES31, QueryWidthAfterBufferResize)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_buffer"));

    constexpr GLint kInitialSize                  = 128;
    constexpr std::array<GLint, 4> kModifiedSizes = {96, 192, 32, 256};

    GLTexture texture;
    glBindTexture(GL_TEXTURE_BUFFER, texture);

    GLBuffer buffer;
    glBindBuffer(GL_TEXTURE_BUFFER, buffer);
    glBufferData(GL_TEXTURE_BUFFER, kInitialSize, nullptr, GL_STATIC_DRAW);

    glTexBufferEXT(GL_TEXTURE_BUFFER, GL_RGBA8, buffer);
    ASSERT_GL_NO_ERROR();

    GLint queryResult = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &queryResult);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(queryResult, kInitialSize / 4);

    for (GLint modifiedSize : kModifiedSizes)
    {
        glBufferData(GL_TEXTURE_BUFFER, modifiedSize, nullptr, GL_STATIC_DRAW);
        glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &queryResult);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(queryResult, modifiedSize / 4);
    }
}

// Test that uploading data to buffer that's in use then using it as texture buffer works.
TEST_P(TextureBufferTestES31, UseAsUBOThenUpdateThenAsTextureBuffer)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_buffer"));

    // Claims to support GL_OES_texture_buffer, but fails compilation of shader because "extension
    // 'GL_OES_texture_buffer' is not supported".  http://anglebug.com/5832
    ANGLE_SKIP_TEST_IF(IsQualcomm() && IsOpenGLES());

    const std::array<GLColor, 4> kInitialData = {GLColor::red, GLColor::red, GLColor::red,
                                                 GLColor::red};
    const std::array<GLColor, 4> kUpdateData  = {GLColor::blue, GLColor::blue, GLColor::blue,
                                                 GLColor::blue};

    GLBuffer buffer;
    glBindBuffer(GL_UNIFORM_BUFFER, buffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(kInitialData), kInitialData.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, buffer);
    EXPECT_GL_NO_ERROR();

    constexpr char kVerifyUBO[] = R"(#version 310 es
precision mediump float;
layout(binding = 0) uniform block {
    uvec4 data;
} ubo;
out vec4 colorOut;
void main()
{
    if (all(equal(ubo.data, uvec4(0xFF0000FFu))))
        colorOut = vec4(0, 1.0, 0, 1.0);
    else
        colorOut = vec4(1.0, 0, 0, 1.0);
})";

    ANGLE_GL_PROGRAM(verifyUbo, essl31_shaders::vs::Simple(), kVerifyUBO);
    drawQuad(verifyUbo, essl31_shaders::PositionAttrib(), 0.5);
    EXPECT_GL_NO_ERROR();

    // Update buffer data
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(kInitialData), kUpdateData.data());
    EXPECT_GL_NO_ERROR();

    // Bind as texture buffer
    GLTexture texture;
    glBindTexture(GL_TEXTURE_BUFFER, texture);
    glTexBufferEXT(GL_TEXTURE_BUFFER, GL_RGBA8, buffer);
    EXPECT_GL_NO_ERROR();

    constexpr char kVerifySamplerBuffer[] = R"(#version 310 es
#extension GL_OES_texture_buffer : require
precision mediump float;
uniform highp samplerBuffer s;
out vec4 colorOut;
void main()
{
    colorOut = texelFetch(s, 0);
})";

    ANGLE_GL_PROGRAM(verifySamplerBuffer, essl31_shaders::vs::Simple(), kVerifySamplerBuffer);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    drawQuad(verifySamplerBuffer, essl31_shaders::PositionAttrib(), 0.5);
    EXPECT_GL_NO_ERROR();

    // Make sure both draw calls succeed
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::cyan);
}

// Test that mapping a texture buffer with GL_MAP_INVALIDATE_BUFFER_BIT and writing to it works
// correctly.
TEST_P(TextureBufferTestES31, MapTextureBufferInvalidateThenWrite)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_buffer"));

    // TODO(http://anglebug.com/5832): Claims to support GL_OES_texture_buffer, but fails
    // compilation of shader because "extension 'GL_OES_texture_buffer' is not supported".
    ANGLE_SKIP_TEST_IF(IsQualcomm() && IsOpenGLES());
    // TODO(http://anglebug.com/6396): The OpenGL backend doesn't correctly handle texture buffers
    // being invalidated when mapped.
    ANGLE_SKIP_TEST_IF(IsOpenGL());

    const std::array<GLColor, 4> kInitialData = {GLColor::red, GLColor::red, GLColor::red,
                                                 GLColor::red};
    const std::array<GLColor, 4> kUpdateData  = {GLColor::blue, GLColor::blue, GLColor::blue,
                                                 GLColor::blue};

    GLBuffer buffer;
    glBindBuffer(GL_TEXTURE_BUFFER, buffer);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(kInitialData), kInitialData.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, buffer);
    EXPECT_GL_NO_ERROR();

    // Bind as texture buffer
    GLTexture texture;
    glBindTexture(GL_TEXTURE_BUFFER, texture);
    glTexBufferEXT(GL_TEXTURE_BUFFER, GL_RGBA8, buffer);
    EXPECT_GL_NO_ERROR();

    constexpr char kSamplerBuffer[] = R"(#version 310 es
#extension GL_OES_texture_buffer : require
precision mediump float;
uniform highp samplerBuffer s;
out vec4 colorOut;
void main()
{
    colorOut = texelFetch(s, 0);
})";

    ANGLE_GL_PROGRAM(initialSamplerBuffer, essl31_shaders::vs::Simple(), kSamplerBuffer);
    drawQuad(initialSamplerBuffer, essl31_shaders::PositionAttrib(), 0.5);
    EXPECT_GL_NO_ERROR();

    // Don't read back, so we don't break the render pass.

    // Map the buffer and update it.
    void *mappedBuffer = glMapBufferRange(GL_TEXTURE_BUFFER, 0, sizeof(kInitialData),
                                          GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    memcpy(mappedBuffer, kUpdateData.data(), sizeof(kInitialData));

    glUnmapBuffer(GL_TEXTURE_BUFFER);

    // Draw with the updated buffer data.
    ANGLE_GL_PROGRAM(updateSamplerBuffer, essl31_shaders::vs::Simple(), kSamplerBuffer);
    drawQuad(updateSamplerBuffer, essl31_shaders::PositionAttrib(), 0.5);
    EXPECT_GL_NO_ERROR();

    // Make sure both draw calls succeed
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
}

// Test that calling glBufferData on a buffer that is used as texture buffer still works correctly.
TEST_P(TextureBufferTestES31, TextureBufferThenBufferData)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_buffer"));

    const std::array<GLColor, 4> kInitialData = {GLColor::red, GLColor::red, GLColor::red,
                                                 GLColor::red};
    const std::array<GLColor, 4> kUpdateData  = {GLColor::blue, GLColor::blue, GLColor::blue,
                                                 GLColor::blue};
    // Create buffer and initialize with data
    GLBuffer buffer;
    glBindBuffer(GL_TEXTURE_BUFFER, buffer);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(kInitialData), kInitialData.data(), GL_DYNAMIC_DRAW);

    // Bind as texture buffer
    GLTexture texture;
    glBindTexture(GL_TEXTURE_BUFFER, texture);
    glTexBufferEXT(GL_TEXTURE_BUFFER, GL_RGBA8, buffer);
    EXPECT_GL_NO_ERROR();

    constexpr char kSamplerBuffer[] = R"(#version 310 es
#extension GL_OES_texture_buffer : require
precision mediump float;
uniform highp samplerBuffer s;
out vec4 colorOut;
void main()
{
    colorOut = texelFetch(s, 0);
})";

    ANGLE_GL_PROGRAM(initialSamplerBuffer, essl31_shaders::vs::Simple(), kSamplerBuffer);
    drawQuad(initialSamplerBuffer, essl31_shaders::PositionAttrib(), 0.5);

    // Don't read back, so we keep the original buffer busy. Issue a glBufferData call with same
    // size and nullptr so that the old buffer storage gets orphaned.
    glBufferData(GL_TEXTURE_BUFFER, sizeof(kUpdateData), nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_TEXTURE_BUFFER, 0, sizeof(kUpdateData), kUpdateData.data());

    // Draw with the updated buffer data.
    ANGLE_GL_PROGRAM(updateSamplerBuffer, essl31_shaders::vs::Simple(), kSamplerBuffer);
    drawQuad(updateSamplerBuffer, essl31_shaders::PositionAttrib(), 0.5);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
}

// Test that the correct error is generated if texture buffer support used anyway when not enabled.
TEST_P(TextureBufferTestES31, TestErrorWhenNotEnabled)
{
    ANGLE_SKIP_TEST_IF(IsGLExtensionEnabled("GL_EXT_texture_buffer"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_BUFFER, texture);
    ASSERT_GL_ERROR(GL_INVALID_ENUM);
}

class CopyImageTestES31 : public ANGLETest<>
{
  protected:
    CopyImageTestES31() {}
};

// Test that copies between RGB formats doesn't affect the emulated alpha channel, if any.
TEST_P(CopyImageTestES31, PreserveEmulatedAlpha)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_copy_image"));

    constexpr GLsizei kSize = 1;

    GLTexture src, dst;

    // Set up the textures
    glBindTexture(GL_TEXTURE_2D, src);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, kSize, kSize);

    const GLColor kInitColor(50, 100, 150, 200);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kSize, kSize, GL_RGB, GL_UNSIGNED_BYTE, &kInitColor);

    glBindTexture(GL_TEXTURE_2D, dst);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8UI, kSize, kSize);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Copy from src to dst
    glCopyImageSubDataEXT(src, GL_TEXTURE_2D, 0, 0, 0, 0, dst, GL_TEXTURE_2D, 0, 0, 0, 0, kSize,
                          kSize, 1);

    // Bind dst as image
    glBindImageTexture(0, dst, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);

    // Create a buffer for output
    constexpr GLsizei kBufferSize = kSize * kSize * sizeof(uint32_t) * 4;
    GLBuffer buffer;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, kBufferSize, nullptr, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);

    constexpr char kCS[] = R"(#version 310 es
layout(local_size_x=1, local_size_y=1, local_size_z=1) in;
layout(rgba8ui, binding = 0) readonly uniform highp uimage2D imageIn;
 layout(std140, binding = 1) buffer dataOut {
     uvec4 data[];
 };
void main()
{
    uvec4 color = imageLoad(imageIn, ivec2(0));
    data[0] = color;
})";

    ANGLE_GL_COMPUTE_PROGRAM(program, kCS);
    glUseProgram(program);
    glDispatchCompute(1, 1, 1);
    EXPECT_GL_NO_ERROR();

    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    const uint32_t *ptr = reinterpret_cast<uint32_t *>(
        glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, kBufferSize, GL_MAP_READ_BIT));

    EXPECT_EQ(ptr[0], kInitColor.R);
    EXPECT_EQ(ptr[1], kInitColor.G);
    EXPECT_EQ(ptr[2], kInitColor.B);

    // Expect alpha to be 1, even if the RGB format is emulated with RGBA.
    EXPECT_EQ(ptr[3], 1u);

    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
}

class TextureChangeStorageUploadTest : public ANGLETest<>
{
  protected:
    TextureChangeStorageUploadTest()
    {
        setWindowWidth(256);
        setWindowHeight(256);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testSetUp() override
    {
        mProgram = CompileProgram(essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        glUseProgram(mProgram);

        glClearColor(0, 0, 0, 0);
        glClearDepthf(0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);

        glGenTextures(1, &mTexture);
        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteTextures(1, &mTexture);
        glDeleteProgram(mProgram);
    }

    GLuint mProgram;
    GLint mColorLocation;
    GLuint mTexture;
};

// Verify that respecifying storage and re-uploading doesn't crash.
TEST_P(TextureChangeStorageUploadTest, Basic)
{
    constexpr int kImageSize        = 8;  // 4 doesn't trip ASAN
    constexpr int kSmallerImageSize = kImageSize / 2;
    EXPECT_GT(kImageSize, kSmallerImageSize);
    EXPECT_GT(kSmallerImageSize / 2, 0);

    std::array<GLColor, kImageSize * kImageSize> kColor;

    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kImageSize, kImageSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 kColor.data());
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kSmallerImageSize, kSmallerImageSize);
    // need partial update to sidestep optimizations that remove the full upload
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kSmallerImageSize / 2, kSmallerImageSize / 2, GL_RGBA,
                    GL_UNSIGNED_BYTE, kColor.data());
    EXPECT_GL_NO_ERROR();
}

class ExtraSamplerCubeShadowUseTest : public ANGLETest<>
{
  protected:
    ExtraSamplerCubeShadowUseTest() : ANGLETest() {}

    const char *getVertexShaderSource() { return "#version 300 es\nvoid main() {}"; }

    const char *getFragmentShaderSource()
    {
        return R"(#version 300 es
precision mediump float;

uniform mediump samplerCube var_0002; // this has to be there
uniform highp samplerCubeShadow var_0004; // this has to be a cube shadow sampler
out vec4 color;
void main() {

    vec4 var_0031 = texture(var_0002, vec3(1,1,1));
    ivec2 size = textureSize(var_0004, 0) ;
    var_0031.x += float(size.y);

    color = var_0031;
})";
    }

    void testSetUp() override
    {
        mProgram = CompileProgram(getVertexShaderSource(), getFragmentShaderSource());
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }
        glUseProgram(mProgram);
        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override { glDeleteProgram(mProgram); }

    GLuint mProgram;
};

TEST_P(ExtraSamplerCubeShadowUseTest, Basic)
{
    glDrawArrays(GL_TRIANGLE_FAN, 0, 3);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
#define ES2_EMULATE_COPY_TEX_IMAGE_VIA_SUB()             \
    ES2_OPENGL().enable(Feature::EmulateCopyTexImage2D), \
        ES2_OPENGLES().enable(Feature::EmulateCopyTexImage2D)
#define ES3_EMULATE_COPY_TEX_IMAGE_VIA_SUB()             \
    ES3_OPENGL().enable(Feature::EmulateCopyTexImage2D), \
        ES3_OPENGLES().enable(Feature::EmulateCopyTexImage2D)
#define ES2_EMULATE_COPY_TEX_IMAGE()                                      \
    ES2_OPENGL().enable(Feature::EmulateCopyTexImage2DFromRenderbuffers), \
        ES2_OPENGLES().enable(Feature::EmulateCopyTexImage2DFromRenderbuffers)
#define ES3_EMULATE_COPY_TEX_IMAGE()                                      \
    ES3_OPENGL().enable(Feature::EmulateCopyTexImage2DFromRenderbuffers), \
        ES3_OPENGLES().enable(Feature::EmulateCopyTexImage2DFromRenderbuffers)
ANGLE_INSTANTIATE_TEST(Texture2DTest,
                       ANGLE_ALL_TEST_PLATFORMS_ES2,
                       ES2_EMULATE_COPY_TEX_IMAGE_VIA_SUB(),
                       ES2_EMULATE_COPY_TEX_IMAGE());
ANGLE_INSTANTIATE_TEST_ES2(TextureCubeTest);
ANGLE_INSTANTIATE_TEST_ES2(Texture2DTestWithDrawScale);
ANGLE_INSTANTIATE_TEST_ES2(Sampler2DAsFunctionParameterTest);
ANGLE_INSTANTIATE_TEST_ES2(SamplerArrayTest);
ANGLE_INSTANTIATE_TEST_ES2(SamplerArrayAsFunctionParameterTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture2DTestES3);
ANGLE_INSTANTIATE_TEST_ES3_AND(Texture2DTestES3,
                               ES3_VULKAN().enable(Feature::AllocateNonZeroMemory));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture2DTestES3YUV);
ANGLE_INSTANTIATE_TEST_ES3_AND(Texture2DTestES3YUV,
                               ES3_VULKAN().enable(Feature::PreferLinearFilterForYUV));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture2DTestES3RobustInit);
ANGLE_INSTANTIATE_TEST_ES3(Texture2DTestES3RobustInit);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture2DTestES31PPO);
ANGLE_INSTANTIATE_TEST_ES31(Texture2DTestES31PPO);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture2DBaseMaxTestES3);
ANGLE_INSTANTIATE_TEST_ES3(Texture2DBaseMaxTestES3);

ANGLE_INSTANTIATE_TEST_ES2(Texture3DTestES2);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture3DTestES3);
ANGLE_INSTANTIATE_TEST_ES3(Texture3DTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture2DIntegerAlpha1TestES3);
ANGLE_INSTANTIATE_TEST_ES3(Texture2DIntegerAlpha1TestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture2DUnsignedIntegerAlpha1TestES3);
ANGLE_INSTANTIATE_TEST_ES3(Texture2DUnsignedIntegerAlpha1TestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ShadowSamplerPlusSampler3DTestES3);
ANGLE_INSTANTIATE_TEST_ES3(ShadowSamplerPlusSampler3DTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SamplerTypeMixTestES3);
ANGLE_INSTANTIATE_TEST_ES3(SamplerTypeMixTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture2DArrayTestES3);
ANGLE_INSTANTIATE_TEST_ES3(Texture2DArrayTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TextureSizeTextureArrayTest);
ANGLE_INSTANTIATE_TEST_ES3(TextureSizeTextureArrayTest);

ANGLE_INSTANTIATE_TEST_ES2(SamplerInStructTest);
ANGLE_INSTANTIATE_TEST_ES2(SamplerInStructAsFunctionParameterTest);
ANGLE_INSTANTIATE_TEST_ES2(SamplerInStructArrayAsFunctionParameterTest);
ANGLE_INSTANTIATE_TEST_ES2(SamplerInNestedStructAsFunctionParameterTest);
ANGLE_INSTANTIATE_TEST_ES2(SamplerInStructAndOtherVariableTest);
ANGLE_INSTANTIATE_TEST_ES2(TextureAnisotropyTest);
ANGLE_INSTANTIATE_TEST_ES2(TextureBorderClampTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TextureBorderClampTestES3);
ANGLE_INSTANTIATE_TEST_ES3(TextureBorderClampTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TextureBorderClampIntegerTestES3);
ANGLE_INSTANTIATE_TEST_ES3(TextureBorderClampIntegerTestES3);

ANGLE_INSTANTIATE_TEST_ES2(TextureLimitsTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture2DNorm16TestES3);
ANGLE_INSTANTIATE_TEST_ES3(Texture2DNorm16TestES3);

ANGLE_INSTANTIATE_TEST(Texture2DRGTest,
                       ANGLE_ALL_TEST_PLATFORMS_ES2,
                       ANGLE_ALL_TEST_PLATFORMS_ES3,
                       ES2_EMULATE_COPY_TEX_IMAGE_VIA_SUB(),
                       ES3_EMULATE_COPY_TEX_IMAGE_VIA_SUB(),
                       ES2_EMULATE_COPY_TEX_IMAGE(),
                       ES3_EMULATE_COPY_TEX_IMAGE());

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture2DFloatTestES3);
ANGLE_INSTANTIATE_TEST_ES3(Texture2DFloatTestES3);

ANGLE_INSTANTIATE_TEST_ES2(Texture2DFloatTestES2);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TextureCubeTestES3);
ANGLE_INSTANTIATE_TEST_ES3(TextureCubeTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture2DIntegerTestES3);
ANGLE_INSTANTIATE_TEST_ES3(Texture2DIntegerTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TextureCubeIntegerTestES3);
ANGLE_INSTANTIATE_TEST_ES3(TextureCubeIntegerTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TextureCubeIntegerEdgeTestES3);
ANGLE_INSTANTIATE_TEST_ES3(TextureCubeIntegerEdgeTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture2DIntegerProjectiveOffsetTestES3);
ANGLE_INSTANTIATE_TEST_ES3(Texture2DIntegerProjectiveOffsetTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture2DArrayIntegerTestES3);
ANGLE_INSTANTIATE_TEST_ES3(Texture2DArrayIntegerTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture3DIntegerTestES3);
ANGLE_INSTANTIATE_TEST_ES3(Texture3DIntegerTestES3);

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(Texture2DDepthTest);
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(PBOCompressedTextureTest);
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(ETC1CompressedTextureTest);
ANGLE_INSTANTIATE_TEST_ES3(PBOCompressedTexture3DTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TextureBufferTestES31);
ANGLE_INSTANTIATE_TEST_ES31(TextureBufferTestES31);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CopyImageTestES31);
ANGLE_INSTANTIATE_TEST_ES31(CopyImageTestES31);

ANGLE_INSTANTIATE_TEST_ES3(TextureChangeStorageUploadTest);

ANGLE_INSTANTIATE_TEST_ES3(ExtraSamplerCubeShadowUseTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Texture2DTestES31);
ANGLE_INSTANTIATE_TEST_ES31(Texture2DTestES31);

}  // anonymous namespace
