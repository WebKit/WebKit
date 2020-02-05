//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include "common/mathutil.h"
#include "platform/FeaturesD3D.h"

using namespace angle;

class DepthStencilFormatsTestBase : public ANGLETest
{
  protected:
    DepthStencilFormatsTestBase()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    bool checkTexImageFormatSupport(GLenum format, GLenum type)
    {
        EXPECT_GL_NO_ERROR();

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, format, 1, 1, 0, format, type, nullptr);
        glDeleteTextures(1, &tex);

        return (glGetError() == GL_NO_ERROR);
    }

    bool checkTexStorageFormatSupport(GLenum internalFormat)
    {
        EXPECT_GL_NO_ERROR();

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexStorage2DEXT(GL_TEXTURE_2D, 1, internalFormat, 1, 1);
        glDeleteTextures(1, &tex);

        return (glGetError() == GL_NO_ERROR);
    }

    bool checkRenderbufferFormatSupport(GLenum internalFormat)
    {
        EXPECT_GL_NO_ERROR();

        GLuint rb = 0;
        glGenRenderbuffers(1, &rb);
        glBindRenderbuffer(GL_RENDERBUFFER, rb);
        glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, 1, 1);
        glDeleteRenderbuffers(1, &rb);

        return (glGetError() == GL_NO_ERROR);
    }

    void verifyDepthRenderBuffer(GLenum internalFormat)
    {
        GLTexture tex;
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        ASSERT_GL_NO_ERROR();

        GLRenderbuffer rbDepth;
        glBindRenderbuffer(GL_RENDERBUFFER, rbDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, 1, 1);
        ASSERT_GL_NO_ERROR();

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbDepth);

        EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
        ASSERT_GL_NO_ERROR();

        ANGLE_GL_PROGRAM(programRed, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_GEQUAL);
        glClearDepthf(0.99f);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Pass Depth Test and draw red
        float depthValue = 1.0f;
        drawQuad(programRed.get(), essl1_shaders::PositionAttrib(), depthValue * 2 - 1);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

        ASSERT_GL_NO_ERROR();

        ANGLE_GL_PROGRAM(programGreen, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());

        // Fail Depth Test and color buffer is unchanged
        depthValue = 0.98f;
        drawQuad(programGreen.get(), essl1_shaders::PositionAttrib(), depthValue * 2 - 1);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

        ASSERT_GL_NO_ERROR();

        ANGLE_GL_PROGRAM(programBlue, essl1_shaders::vs::Simple(), essl1_shaders::fs::Blue());
        glClearDepthf(0.0f);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Pass Depth Test and draw blue
        depthValue = 0.01f;
        drawQuad(programBlue.get(), essl1_shaders::PositionAttrib(), depthValue * 2 - 1);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

        glDisable(GL_DEPTH_TEST);
        ASSERT_GL_NO_ERROR();
    }

    void testSetUp() override
    {
        constexpr char kVS[] = R"(precision highp float;
attribute vec4 position;
varying vec2 texcoord;

void main()
{
    gl_Position = position;
    texcoord = (position.xy * 0.5) + 0.5;
})";

        constexpr char kFS[] = R"(precision highp float;
uniform sampler2D tex;
varying vec2 texcoord;

void main()
{
    gl_FragColor = texture2D(tex, texcoord);
})";

        mProgram = CompileProgram(kVS, kFS);
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        mTextureUniformLocation = glGetUniformLocation(mProgram, "tex");
        EXPECT_NE(-1, mTextureUniformLocation);

        glGenTextures(1, &mTexture);
        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteProgram(mProgram);
        glDeleteTextures(1, &mTexture);
    }

    GLuint mProgram;
    GLuint mTexture;
    GLint mTextureUniformLocation;
};

class DepthStencilFormatsTest : public DepthStencilFormatsTestBase
{};

class DepthStencilFormatsTestES3 : public DepthStencilFormatsTestBase
{};

TEST_P(DepthStencilFormatsTest, DepthTexture)
{
    bool shouldHaveTextureSupport = (IsGLExtensionEnabled("GL_ANGLE_depth_texture") ||
                                     IsGLExtensionEnabled("GL_OES_depth_texture"));

    EXPECT_EQ(shouldHaveTextureSupport,
              checkTexImageFormatSupport(GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT));
    EXPECT_EQ(shouldHaveTextureSupport,
              checkTexImageFormatSupport(GL_DEPTH_COMPONENT, GL_UNSIGNED_INT));

    if (IsGLExtensionEnabled("GL_EXT_texture_storage"))
    {
        EXPECT_EQ(shouldHaveTextureSupport, checkTexStorageFormatSupport(GL_DEPTH_COMPONENT16));
        EXPECT_EQ(shouldHaveTextureSupport, checkTexStorageFormatSupport(GL_DEPTH_COMPONENT32_OES));
    }
}

TEST_P(DepthStencilFormatsTest, PackedDepthStencil)
{
    // Expected to fail in D3D9 if GL_OES_packed_depth_stencil is not present.
    // Expected to fail in D3D11 if GL_OES_packed_depth_stencil or GL_ANGLE_depth_texture is not
    // present.

    bool shouldHaveRenderbufferSupport = IsGLExtensionEnabled("GL_OES_packed_depth_stencil");
    EXPECT_EQ(shouldHaveRenderbufferSupport,
              checkRenderbufferFormatSupport(GL_DEPTH24_STENCIL8_OES));

    bool shouldHaveTextureSupport = ((IsGLExtensionEnabled("GL_OES_packed_depth_stencil") ||
                                      IsGLExtensionEnabled("GL_OES_depth_texture_cube_map")) &&
                                     IsGLExtensionEnabled("GL_OES_depth_texture")) ||
                                    IsGLExtensionEnabled("GL_ANGLE_depth_texture");
    EXPECT_EQ(shouldHaveTextureSupport,
              checkTexImageFormatSupport(GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES));

    if (IsGLExtensionEnabled("GL_EXT_texture_storage"))
    {
        bool shouldHaveTexStorageSupport = IsGLExtensionEnabled("GL_OES_packed_depth_stencil") ||
                                           IsGLExtensionEnabled("GL_ANGLE_depth_texture");
        EXPECT_EQ(shouldHaveTexStorageSupport,
                  checkTexStorageFormatSupport(GL_DEPTH24_STENCIL8_OES));
    }
}

// This test will initialize a depth texture and then render with it and verify
// pixel correctness.
// This is modeled after webgl-depth-texture.html
TEST_P(DepthStencilFormatsTest, DepthTextureRender)
{
    constexpr char kVS[] = R"(attribute vec4 a_position;
void main()
{
    gl_Position = a_position;
})";

    constexpr char kFS[] = R"(precision mediump float;
uniform sampler2D u_texture;
uniform vec2 u_resolution;
void main()
{
    vec2 texcoord = (gl_FragCoord.xy - vec2(0.5)) / (u_resolution - vec2(1.0));
    gl_FragColor = texture2D(u_texture, texcoord);
})";

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_depth_texture") &&
                       !IsGLExtensionEnabled("GL_ANGLE_depth_texture"));

    bool depthTextureCubeSupport = IsGLExtensionEnabled("GL_OES_depth_texture_cube_map");

    // http://anglebug.com/3454
    ANGLE_SKIP_TEST_IF(IsIntel() && IsWindows() && IsD3D9());

    const int res     = 2;
    const int destRes = 4;
    GLint resolution;

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    glUseProgram(program);
    resolution = glGetUniformLocation(program, "u_resolution");
    ASSERT_NE(-1, resolution);
    glUniform2f(resolution, static_cast<float>(destRes), static_cast<float>(destRes));

    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    float verts[] = {
        1, 1, 1, -1, 1, 0, -1, -1, -1, 1, 1, 1, -1, -1, -1, 1, -1, 0,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);

    // OpenGL ES does not have a FLIPY PixelStore attribute
    // glPixelStorei(GL_UNPACK_FLIP)

    enum ObjType
    {
        GL,
        EXT
    };
    struct TypeInfo
    {
        ObjType obj;
        GLuint attachment;
        GLuint format;
        GLuint type;
        void *data;
        int depthBits;
        int stencilBits;
    };

    GLuint fakeData[10] = {0};

    std::vector<TypeInfo> types = {
        {ObjType::GL, GL_DEPTH_ATTACHMENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, fakeData, 16, 0},
        {ObjType::GL, GL_DEPTH_ATTACHMENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, fakeData, 16, 0},
        {ObjType::EXT, GL_DEPTH_STENCIL_ATTACHMENT, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8_OES,
         fakeData, 24, 8},
    };

    for (const TypeInfo &type : types)
    {
        GLTexture cubeTex;
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTex);
        ASSERT_GL_NO_ERROR();

        std::vector<GLuint> targets{GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                                    GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                                    GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

        for (const GLuint target : targets)
        {
            glTexImage2D(target, 0, type.format, 1, 1, 0, type.format, type.type, nullptr);
            if (depthTextureCubeSupport)
            {
                ASSERT_GL_NO_ERROR();
            }
            else
            {
                EXPECT_GL_ERROR(GL_INVALID_OPERATION);
            }
        }

        std::vector<GLuint> filterModes = {GL_LINEAR, GL_NEAREST};

        const bool supportPackedDepthStencilFramebuffer = getClientMajorVersion() >= 3;

        for (const GLuint filterMode : filterModes)
        {
            GLTexture tex;
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterMode);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterMode);

            // test level > 0
            glTexImage2D(GL_TEXTURE_2D, 1, type.format, 1, 1, 0, type.format, type.type, nullptr);
            if (IsGLExtensionEnabled("GL_OES_depth_texture"))
            {
                EXPECT_GL_NO_ERROR();
            }
            else
            {
                EXPECT_GL_ERROR(GL_INVALID_OPERATION);
            }

            // test with data
            glTexImage2D(GL_TEXTURE_2D, 0, type.format, 1, 1, 0, type.format, type.type, type.data);
            if (IsGLExtensionEnabled("GL_OES_depth_texture"))
            {
                EXPECT_GL_NO_ERROR();
            }
            else
            {
                EXPECT_GL_ERROR(GL_INVALID_OPERATION);
            }

            // test copyTexImage2D
            glCopyTexImage2D(GL_TEXTURE_2D, 0, type.format, 0, 0, 1, 1, 0);
            GLuint error = glGetError();
            ASSERT_TRUE(error == GL_INVALID_ENUM || error == GL_INVALID_OPERATION);

            // test real thing
            glTexImage2D(GL_TEXTURE_2D, 0, type.format, res, res, 0, type.format, type.type,
                         nullptr);
            EXPECT_GL_NO_ERROR();

            // test texSubImage2D
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, type.format, type.type, type.data);
            if (IsGLExtensionEnabled("GL_OES_depth_texture"))
            {
                EXPECT_GL_NO_ERROR();
            }
            else
            {
                EXPECT_GL_ERROR(GL_INVALID_OPERATION);
            }

            // test copyTexSubImage2D
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
            EXPECT_GL_ERROR(GL_INVALID_OPERATION);

            // test generateMipmap
            glGenerateMipmap(GL_TEXTURE_2D);
            EXPECT_GL_ERROR(GL_INVALID_OPERATION);

            GLuint fbo = 0;
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            if (type.depthBits > 0 && type.stencilBits > 0 && !supportPackedDepthStencilFramebuffer)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
                EXPECT_GL_NO_ERROR();
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex,
                                       0);
                EXPECT_GL_NO_ERROR();
            }
            else
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, type.attachment, GL_TEXTURE_2D, tex, 0);
                EXPECT_GL_NO_ERROR();
            }

            // Ensure DEPTH_BITS returns >= 16 bits for UNSIGNED_SHORT and UNSIGNED_INT, >= 24
            // UNSIGNED_INT_24_8_WEBGL. If there is stencil, ensure STENCIL_BITS reports >= 8 for
            // UNSIGNED_INT_24_8_WEBGL.

            GLint depthBits = 0;
            glGetIntegerv(GL_DEPTH_BITS, &depthBits);
            EXPECT_GE(depthBits, type.depthBits);

            GLint stencilBits = 0;
            glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
            EXPECT_GE(stencilBits, type.stencilBits);

            // TODO: remove this check if the spec is updated to require these combinations to work.
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                // try adding a color buffer.
                GLuint colorTex = 0;
                glGenTextures(1, &colorTex);
                glBindTexture(GL_TEXTURE_2D, colorTex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, res, res, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             nullptr);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                       colorTex, 0);
                EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
            }

            EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

            // use the default texture to render with while we return to the depth texture.
            glBindTexture(GL_TEXTURE_2D, 0);

            /* Setup 2x2 depth texture:
             * 1 0.6 0.8
             * |
             * 0 0.2 0.4
             *    0---1
             */
            const GLfloat d00 = 0.2;
            const GLfloat d01 = 0.4;
            const GLfloat d10 = 0.6;
            const GLfloat d11 = 0.8;
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, 0, 1, 1);
            glClearDepthf(d00);
            glClear(GL_DEPTH_BUFFER_BIT);
            glScissor(1, 0, 1, 1);
            glClearDepthf(d10);
            glClear(GL_DEPTH_BUFFER_BIT);
            glScissor(0, 1, 1, 1);
            glClearDepthf(d01);
            glClear(GL_DEPTH_BUFFER_BIT);
            glScissor(1, 1, 1, 1);
            glClearDepthf(d11);
            glClear(GL_DEPTH_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);

            // render the depth texture.
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, destRes, destRes);
            glBindTexture(GL_TEXTURE_2D, tex);
            glDisable(GL_DITHER);
            glEnable(GL_DEPTH_TEST);
            glClearColor(1, 0, 0, 1);
            glClearDepthf(1.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            GLubyte actualPixels[destRes * destRes * 4];
            glReadPixels(0, 0, destRes, destRes, GL_RGBA, GL_UNSIGNED_BYTE, actualPixels);
            const GLfloat eps = 0.002;
            std::vector<GLfloat> expectedMin;
            std::vector<GLfloat> expectedMax;
            if (filterMode == GL_NEAREST)
            {
                GLfloat init[] = {d00, d00, d10, d10, d00, d00, d10, d10,
                                  d01, d01, d11, d11, d01, d01, d11, d11};
                expectedMin.insert(expectedMin.begin(), init, init + 16);
                expectedMax.insert(expectedMax.begin(), init, init + 16);

                for (int i = 0; i < 16; i++)
                {
                    expectedMin[i] = expectedMin[i] - eps;
                    expectedMax[i] = expectedMax[i] + eps;
                }
            }
            else
            {
                GLfloat initMin[] = {
                    d00 - eps, d00, d00, d10 - eps, d00,       d00, d00, d10,
                    d00,       d00, d00, d10,       d01 - eps, d01, d01, d11 - eps,
                };
                GLfloat initMax[] = {
                    d00 + eps, d10, d10, d10 + eps, d01,       d11, d11, d11,
                    d01,       d11, d11, d11,       d01 + eps, d11, d11, d11 + eps,
                };
                expectedMin.insert(expectedMin.begin(), initMin, initMin + 16);
                expectedMax.insert(expectedMax.begin(), initMax, initMax + 16);
            }
            for (int yy = 0; yy < destRes; ++yy)
            {
                for (int xx = 0; xx < destRes; ++xx)
                {
                    const int t        = xx + destRes * yy;
                    const GLfloat was  = (GLfloat)(actualPixels[4 * t] / 255.0);  // 4bpp
                    const GLfloat eMin = expectedMin[t];
                    const GLfloat eMax = expectedMax[t];
                    EXPECT_TRUE(was >= eMin && was <= eMax)
                        << "At " << xx << ", " << yy << ", expected within [" << eMin << ", "
                        << eMax << "] was " << was;
                }
            }

            // check limitations
            // Note: What happens if current attachment type is GL_DEPTH_STENCIL_ATTACHMENT
            // and you try to call glFramebufferTexture2D with GL_DEPTH_ATTACHMENT?
            // The webGL test this code came from expected that to fail.
            // I think due to this line in ES3 spec:
            // GL_INVALID_OPERATION is generated if textarget and texture are not compatible
            // However, that's not the behavior I'm seeing, nor does it seem that a depth_stencil
            // buffer isn't compatible with a depth attachment (e.g. stencil is unused).
            if (type.attachment == GL_DEPTH_ATTACHMENT)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, type.attachment, GL_TEXTURE_2D, 0, 0);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                                       tex, 0);
                EXPECT_GLENUM_NE(GL_NO_ERROR, glGetError());
                EXPECT_GLENUM_NE(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
                glClear(GL_DEPTH_BUFFER_BIT);
                EXPECT_GL_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                EXPECT_GL_NO_ERROR();
            }
        }
    }
}

// This test will initialize a frame buffer, attaching a color and 16-bit depth buffer,
// render to it with depth testing, and verify pixel correctness.
TEST_P(DepthStencilFormatsTest, DepthBuffer16)
{
    verifyDepthRenderBuffer(GL_DEPTH_COMPONENT16);
}

// This test will initialize a frame buffer, attaching a color and 24-bit depth buffer,
// render to it with depth testing, and verify pixel correctness.
TEST_P(DepthStencilFormatsTest, DepthBuffer24)
{
    bool shouldHaveRenderbufferSupport = IsGLExtensionEnabled("GL_OES_depth24");
    EXPECT_EQ(shouldHaveRenderbufferSupport,
              checkRenderbufferFormatSupport(GL_DEPTH_COMPONENT24_OES));

    if (shouldHaveRenderbufferSupport)
    {
        verifyDepthRenderBuffer(GL_DEPTH_COMPONENT24_OES);
    }
}

TEST_P(DepthStencilFormatsTestES3, DrawWithDepth16)
{
    GLushort data[16];
    for (unsigned int i = 0; i < 16; i++)
    {
        data[i] = std::numeric_limits<GLushort>::max();
    }
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, 4, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glUseProgram(mProgram);
    glUniform1i(mTextureUniformLocation, 0);

    glClear(GL_COLOR_BUFFER_BIT);
    drawQuad(mProgram, "position", 0.5f);

    ASSERT_GL_NO_ERROR();

    GLubyte pixel[4];
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);

    // Only require the red and alpha channels have the correct values, the depth texture extensions
    // leave the green and blue channels undefined
    ASSERT_NEAR(255, pixel[0], 2.0);
    ASSERT_EQ(255, pixel[3]);
}

// This test reproduces a driver bug on Intel windows platforms on driver version
// from 4815 to 4901.
// When rendering with Stencil buffer enabled and depth buffer disabled, large
// viewport will lead to memory leak and driver crash. And the pixel result
// is a random value.
TEST_P(DepthStencilFormatsTestES3, DrawWithLargeViewport)
{
    ANGLE_SKIP_TEST_IF(IsIntel() && (IsOSX() || IsWindows()));

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());

    glEnable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);

    // The iteration is to reproduce memory leak when rendering several times.
    for (int i = 0; i < 10; ++i)
    {
        // Create offscreen fbo and its color attachment and depth stencil attachment.
        GLTexture framebufferColorTexture;
        glBindTexture(GL_TEXTURE_2D, framebufferColorTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, getWindowWidth(), getWindowHeight());
        ASSERT_GL_NO_ERROR();

        GLTexture framebufferStencilTexture;
        glBindTexture(GL_TEXTURE_2D, framebufferStencilTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, getWindowWidth(), getWindowHeight());
        ASSERT_GL_NO_ERROR();

        GLFramebuffer fb;
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               framebufferColorTexture, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               framebufferStencilTexture, 0);

        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        ASSERT_GL_NO_ERROR();

        GLint kStencilRef = 4;
        glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, kStencilRef, 0xFF);

        float viewport[2];
        glGetFloatv(GL_MAX_VIEWPORT_DIMS, viewport);

        glViewport(0, 0, static_cast<GLsizei>(viewport[0]), static_cast<GLsizei>(viewport[1]));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb);

        drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.0f);
        ASSERT_GL_NO_ERROR();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
        ASSERT_GL_NO_ERROR();
    }
}

// Verify that stencil component of depth texture is uploaded
TEST_P(DepthStencilFormatsTest, VerifyDepthStencilUploadData)
{
    // http://anglebug.com/3683
    // When bug is resolved we can remove this skip.
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsAndroid());

    // http://anglebug.com/3689
    ANGLE_SKIP_TEST_IF(IsWindows() && IsVulkan() && IsAMD());

    bool shouldHaveTextureSupport = (IsGLExtensionEnabled("GL_OES_packed_depth_stencil") &&
                                     IsGLExtensionEnabled("GL_OES_depth_texture")) ||
                                    (getClientMajorVersion() >= 3);

    ANGLE_SKIP_TEST_IF(!shouldHaveTextureSupport ||
                       !checkTexImageFormatSupport(GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES));

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());

    glEnable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glViewport(0, 0, getWindowWidth(), getWindowHeight());
    glClearColor(0, 0, 0, 1);

    // Create offscreen fbo and its color attachment and depth stencil attachment.
    GLTexture framebufferColorTexture;
    glBindTexture(GL_TEXTURE_2D, framebufferColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    // drawQuad's depth range is -1.0 to 1.0, so a depth value of 0.5 (0x7fffff) matches a drawQuad
    // depth of 0.0.
    std::vector<GLuint> depthStencilData(getWindowWidth() * getWindowHeight(), 0x7fffffA9);
    GLTexture framebufferStencilTexture;
    glBindTexture(GL_TEXTURE_2D, framebufferStencilTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL, getWindowWidth(), getWindowHeight(), 0,
                 GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8_OES, depthStencilData.data());
    ASSERT_GL_NO_ERROR();

    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           framebufferColorTexture, 0);

    if (getClientMajorVersion() >= 3)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               framebufferStencilTexture, 0);
        ASSERT_GL_NO_ERROR();
    }
    else
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                               framebufferStencilTexture, 0);
        ASSERT_GL_NO_ERROR();
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               framebufferStencilTexture, 0);
        ASSERT_GL_NO_ERROR();
    }

    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    GLint kStencilRef = 0xA9;
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilFunc(GL_EQUAL, kStencilRef, 0xFF);

    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    glClear(GL_COLOR_BUFFER_BIT);

    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 1.0f);
    ASSERT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth(), getWindowHeight(), GLColor::red);
    ASSERT_GL_NO_ERROR();

    // Check Z values

    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    drawQuad(program.get(), essl1_shaders::PositionAttrib(), -0.1f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth(), getWindowHeight(), GLColor::red);

    glClear(GL_COLOR_BUFFER_BIT);

    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.1f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth(), getWindowHeight(), GLColor::black);
}

// Verify that depth texture's data can be uploaded correctly
TEST_P(DepthStencilFormatsTest, VerifyDepth32UploadData)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_depth_texture"));

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    // normalized 0.99 = 0xfd70a3d6
    std::vector<GLuint> depthData(1, 0xfd70a3d6);
    GLTexture rbDepth;
    glBindTexture(GL_TEXTURE_2D, rbDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1, 1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
                 depthData.data());
    ASSERT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rbDepth, 0);

    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    ASSERT_GL_NO_ERROR();

    ANGLE_GL_PROGRAM(programRed, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Fail Depth Test and color buffer is unchanged
    float depthValue = 0.98f;
    drawQuad(programRed.get(), essl1_shaders::PositionAttrib(), depthValue * 2 - 1);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Pass Depth Test and draw red
    depthValue = 1.0f;
    drawQuad(programRed.get(), essl1_shaders::PositionAttrib(), depthValue * 2 - 1);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    ASSERT_GL_NO_ERROR();

    // Change depth texture data
    glBindTexture(GL_TEXTURE_2D, rbDepth);
    depthData[0] = 0x7fffffff;  // 0.5
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
                    depthData.data());
    ASSERT_GL_NO_ERROR();

    ANGLE_GL_PROGRAM(programGreen, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());

    // Fail Depth Test and color buffer is unchanged
    depthValue = 0.48f;
    drawQuad(programGreen.get(), essl1_shaders::PositionAttrib(), depthValue * 2 - 1);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    ASSERT_GL_NO_ERROR();

    ANGLE_GL_PROGRAM(programBlue, essl1_shaders::vs::Simple(), essl1_shaders::fs::Blue());
    glClearDepthf(0.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Pass Depth Test and draw blue
    depthValue = 0.01f;
    drawQuad(programBlue.get(), essl1_shaders::PositionAttrib(), depthValue * 2 - 1);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    glDisable(GL_DEPTH_TEST);
    ASSERT_GL_NO_ERROR();
}

// Verify that 16 bits depth texture's data can be uploaded correctly
TEST_P(DepthStencilFormatsTest, VerifyDepth16UploadData)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_depth_texture"));

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    // normalized 0.99 = 0xfd6f
    std::vector<GLushort> depthData(1, 0xfd6f);
    GLTexture rbDepth;
    glBindTexture(GL_TEXTURE_2D, rbDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1, 1, 0, GL_DEPTH_COMPONENT,
                 GL_UNSIGNED_SHORT, depthData.data());
    ASSERT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rbDepth, 0);

    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    ASSERT_GL_NO_ERROR();

    ANGLE_GL_PROGRAM(programRed, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Fail Depth Test and color buffer is unchanged
    float depthValue = 0.98f;
    drawQuad(programRed.get(), essl1_shaders::PositionAttrib(), depthValue * 2 - 1);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Pass Depth Test and draw red
    depthValue = 1.0f;
    drawQuad(programRed.get(), essl1_shaders::PositionAttrib(), depthValue * 2 - 1);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    ASSERT_GL_NO_ERROR();

    // Change depth texture data
    glBindTexture(GL_TEXTURE_2D, rbDepth);
    depthData[0] = 0x7fff;  // 0.5
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,
                    depthData.data());
    ASSERT_GL_NO_ERROR();

    ANGLE_GL_PROGRAM(programGreen, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());

    // Fail Depth Test and color buffer is unchanged
    depthValue = 0.48f;
    drawQuad(programGreen.get(), essl1_shaders::PositionAttrib(), depthValue * 2 - 1);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    ASSERT_GL_NO_ERROR();

    ANGLE_GL_PROGRAM(programBlue, essl1_shaders::vs::Simple(), essl1_shaders::fs::Blue());
    glClearDepthf(0.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Pass Depth Test and draw blue
    depthValue = 0.01f;
    drawQuad(programBlue.get(), essl1_shaders::PositionAttrib(), depthValue * 2 - 1);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    glDisable(GL_DEPTH_TEST);
    ASSERT_GL_NO_ERROR();
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2(DepthStencilFormatsTest);
ANGLE_INSTANTIATE_TEST_ES3(DepthStencilFormatsTestES3);

class TinyDepthStencilWorkaroundTest : public ANGLETest
{
  public:
    TinyDepthStencilWorkaroundTest()
    {
        setWindowWidth(512);
        setWindowHeight(512);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    // Override the features to enable "tiny" depth/stencil textures.
    void overrideWorkaroundsD3D(FeaturesD3D *features) override
    {
        features->overrideFeatures({"emulate_tiny_stencil_textures"}, true);
    }
};

// Tests that the tiny depth stencil textures workaround does not "stick" depth textures.
// http://anglebug.com/1664
TEST_P(TinyDepthStencilWorkaroundTest, DepthTexturesStick)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF((IsAndroid() && IsOpenGLES()) || (IsLinux() && IsVulkan()));
    ANGLE_SKIP_TEST_IF(isSwiftshader());
    constexpr char kDrawVS[] =
        "#version 100\n"
        "attribute vec3 vertex;\n"
        "void main () {\n"
        "  gl_Position = vec4(vertex.x, vertex.y, vertex.z * 2.0 - 1.0, 1);\n"
        "}\n";

    ANGLE_GL_PROGRAM(drawProgram, kDrawVS, essl1_shaders::fs::Red());

    constexpr char kBlitVS[] =
        "#version 100\n"
        "attribute vec2 vertex;\n"
        "varying vec2 position;\n"
        "void main () {\n"
        "  position = vertex * .5 + .5;\n"
        "  gl_Position = vec4(vertex, 0, 1);\n"
        "}\n";

    constexpr char kBlitFS[] =
        "#version 100\n"
        "precision mediump float;\n"
        "uniform sampler2D texture;\n"
        "varying vec2 position;\n"
        "void main () {\n"
        "  gl_FragColor = vec4 (texture2D (texture, position).rrr, 1.);\n"
        "}\n";

    ANGLE_GL_PROGRAM(blitProgram, kBlitVS, kBlitFS);

    GLint blitTextureLocation = glGetUniformLocation(blitProgram.get(), "texture");
    ASSERT_NE(-1, blitTextureLocation);

    GLTexture colorTex;
    glBindTexture(GL_TEXTURE_2D, colorTex.get());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLTexture depthTex;
    glBindTexture(GL_TEXTURE_2D, depthTex.get());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    ASSERT_EQ(getWindowWidth(), getWindowHeight());
    int levels = gl::log2(getWindowWidth());
    for (int mipLevel = 0; mipLevel <= levels; ++mipLevel)
    {
        int size = getWindowWidth() >> mipLevel;
        glTexImage2D(GL_TEXTURE_2D, mipLevel, GL_DEPTH_STENCIL, size, size, 0, GL_DEPTH_STENCIL,
                     GL_UNSIGNED_INT_24_8_OES, nullptr);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    ASSERT_GL_NO_ERROR();

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex.get(), 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex.get(), 0);

    ASSERT_GL_NO_ERROR();

    glDepthRangef(0.0f, 1.0f);
    glViewport(0, 0, getWindowWidth(), getWindowHeight());
    glClearColor(0, 0, 0, 1);

    // Draw loop.
    for (unsigned int frame = 0; frame < 3; ++frame)
    {
        // draw into FBO
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        float depth = ((frame % 2 == 0) ? 0.0f : 1.0f);
        drawQuad(drawProgram.get(), "vertex", depth);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // blit FBO
        glDisable(GL_DEPTH_TEST);

        glUseProgram(blitProgram.get());
        glUniform1i(blitTextureLocation, 0);
        glBindTexture(GL_TEXTURE_2D, depthTex.get());

        drawQuad(blitProgram.get(), "vertex", 0.5f);

        Vector4 depthVec(depth, depth, depth, 1);
        GLColor depthColor(depthVec);

        EXPECT_PIXEL_COLOR_NEAR(0, 0, depthColor, 1);
        ASSERT_GL_NO_ERROR();
    }
}

ANGLE_INSTANTIATE_TEST_ES3(TinyDepthStencilWorkaroundTest);
