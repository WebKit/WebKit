//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureMultisampleTest: Tests of multisampled texture

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"
#include "util/shader_utils.h"

using namespace angle;

namespace
{
// Sample positions of d3d standard pattern. Some of the sample positions might not the same as
// opengl.
using SamplePositionsArray                                            = std::array<float, 32>;
static constexpr std::array<SamplePositionsArray, 5> kSamplePositions = {
    {{{0.5f, 0.5f}},
     {{0.75f, 0.75f, 0.25f, 0.25f}},
     {{0.375f, 0.125f, 0.875f, 0.375f, 0.125f, 0.625f, 0.625f, 0.875f}},
     {{0.5625f, 0.3125f, 0.4375f, 0.6875f, 0.8125f, 0.5625f, 0.3125f, 0.1875f, 0.1875f, 0.8125f,
       0.0625f, 0.4375f, 0.6875f, 0.9375f, 0.9375f, 0.0625f}},
     {{0.5625f, 0.5625f, 0.4375f, 0.3125f, 0.3125f, 0.625f,  0.75f,   0.4375f,
       0.1875f, 0.375f,  0.625f,  0.8125f, 0.8125f, 0.6875f, 0.6875f, 0.1875f,
       0.375f,  0.875f,  0.5f,    0.0625f, 0.25f,   0.125f,  0.125f,  0.75f,
       0.0f,    0.5f,    0.9375f, 0.25f,   0.875f,  0.9375f, 0.0625f, 0.0f}}}};

class TextureMultisampleTest : public ANGLETest
{
  protected:
    TextureMultisampleTest()
    {
        setWindowWidth(64);
        setWindowHeight(64);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testSetUp() override
    {
        glGenFramebuffers(1, &mFramebuffer);
        glGenTextures(1, &mTexture);

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteFramebuffers(1, &mFramebuffer);
        mFramebuffer = 0;
        glDeleteTextures(1, &mTexture);
        mTexture = 0;
    }

    void texStorageMultisample(GLenum target,
                               GLint samples,
                               GLenum format,
                               GLsizei width,
                               GLsizei height,
                               GLboolean fixedsamplelocations);

    void getTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params);
    void getTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params);

    void getMultisamplefv(GLenum pname, GLuint index, GLfloat *val);
    void sampleMaski(GLuint maskNumber, GLbitfield mask);

    GLuint mFramebuffer = 0;
    GLuint mTexture     = 0;

    // Returns a sample count that can be used with the given texture target for all the given
    // formats. Assumes that if format A supports a number of samples N and another format B
    // supports a number of samples M > N then format B also supports number of samples N.
    GLint getSamplesToUse(GLenum texTarget, const std::vector<GLenum> &formats)
    {
        GLint maxSamples = 65536;
        for (GLenum format : formats)
        {
            GLint maxSamplesFormat = 0;
            glGetInternalformativ(texTarget, format, GL_SAMPLES, 1, &maxSamplesFormat);
            maxSamples = std::min(maxSamples, maxSamplesFormat);
        }
        return maxSamples;
    }

    bool lessThanES31MultisampleExtNotSupported()
    {
        return getClientMajorVersion() <= 3 && getClientMinorVersion() < 1 &&
               !EnsureGLExtensionEnabled("GL_ANGLE_texture_multisample");
    }

    const char *multisampleTextureFragmentShader()
    {
        return R"(#version 300 es
#extension GL_ANGLE_texture_multisample : require
precision highp float;
precision highp int;

uniform highp sampler2DMS tex;
uniform int sampleNum;

in vec4 v_position;
out vec4 my_FragColor;

void main() {
    ivec2 texSize = textureSize(tex);
    ivec2 sampleCoords = ivec2((v_position.xy * 0.5 + 0.5) * vec2(texSize.xy - 1));
    my_FragColor = texelFetch(tex, sampleCoords, sampleNum);
}
)";
    }

    const char *blitArrayTextureLayerFragmentShader()
    {
        return R"(#version 310 es
#extension GL_OES_texture_storage_multisample_2d_array : require
precision highp float;
precision highp int;

uniform highp sampler2DMSArray tex;
uniform int layer;
uniform int sampleNum;

in vec4 v_position;
out vec4 my_FragColor;

void main() {
    ivec3 texSize = textureSize(tex);
    ivec2 sampleCoords = ivec2((v_position.xy * 0.5 + 0.5) * vec2(texSize.xy - 1));
    my_FragColor = texelFetch(tex, ivec3(sampleCoords, layer), sampleNum);
}
)";
    }

    const char *blitIntArrayTextureLayerFragmentShader()
    {
        return R"(#version 310 es
#extension GL_OES_texture_storage_multisample_2d_array : require
precision highp float;
precision highp int;

uniform highp isampler2DMSArray tex;
uniform int layer;
uniform int sampleNum;

in vec4 v_position;
out vec4 my_FragColor;

void main() {
    ivec3 texSize = textureSize(tex);
    ivec2 sampleCoords = ivec2((v_position.xy * 0.5 + 0.5) * vec2(texSize.xy - 1));
    my_FragColor = vec4(texelFetch(tex, ivec3(sampleCoords, layer), sampleNum));
}
)";
    }
};

class NegativeTextureMultisampleTest : public TextureMultisampleTest
{
  protected:
    NegativeTextureMultisampleTest() : TextureMultisampleTest() {}
};

class TextureMultisampleArrayWebGLTest : public TextureMultisampleTest
{
  protected:
    TextureMultisampleArrayWebGLTest() : TextureMultisampleTest()
    {
        // These tests run in WebGL mode so we can test with both extension off and on.
        setWebGLCompatibilityEnabled(true);
    }

    // Requests the ANGLE_texture_multisample_array extension and returns true if the operation
    // succeeds.
    bool requestArrayExtension()
    {
        if (IsGLExtensionRequestable("GL_OES_texture_storage_multisample_2d_array"))
        {
            glRequestExtensionANGLE("GL_OES_texture_storage_multisample_2d_array");
        }

        if (!IsGLExtensionEnabled("GL_OES_texture_storage_multisample_2d_array"))
        {
            return false;
        }
        return true;
    }
};

void TextureMultisampleTest::texStorageMultisample(GLenum target,
                                                   GLint samples,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLboolean fixedsamplelocations)
{
    if (getClientMajorVersion() <= 3 && getClientMinorVersion() < 1 &&
        EnsureGLExtensionEnabled("GL_ANGLE_texture_multisample"))
    {
        glTexStorage2DMultisampleANGLE(target, samples, internalformat, width, height,
                                       fixedsamplelocations);
    }
    else
    {
        glTexStorage2DMultisample(target, samples, internalformat, width, height,
                                  fixedsamplelocations);
    }
}

void TextureMultisampleTest::getTexLevelParameterfv(GLenum target,
                                                    GLint level,
                                                    GLenum pname,
                                                    GLfloat *params)
{
    if (getClientMajorVersion() <= 3 && getClientMinorVersion() < 1 &&
        EnsureGLExtensionEnabled("GL_ANGLE_texture_multisample"))
    {
        glGetTexLevelParameterfvANGLE(target, level, pname, params);
    }
    else
    {
        glGetTexLevelParameterfv(target, level, pname, params);
    }
}

void TextureMultisampleTest::getTexLevelParameteriv(GLenum target,
                                                    GLint level,
                                                    GLenum pname,
                                                    GLint *params)
{
    if (getClientMajorVersion() <= 3 && getClientMinorVersion() < 1 &&
        EnsureGLExtensionEnabled("GL_ANGLE_texture_multisample"))
    {
        glGetTexLevelParameterivANGLE(target, level, pname, params);
    }
    else
    {
        glGetTexLevelParameteriv(target, level, pname, params);
    }
}

void TextureMultisampleTest::getMultisamplefv(GLenum pname, GLuint index, GLfloat *val)
{
    if (getClientMajorVersion() <= 3 && getClientMinorVersion() < 1 &&
        EnsureGLExtensionEnabled("GL_ANGLE_texture_multisample"))
    {
        glGetMultisamplefvANGLE(pname, index, val);
    }
    else
    {
        glGetMultisamplefv(pname, index, val);
    }
}

void TextureMultisampleTest::sampleMaski(GLuint maskNumber, GLbitfield mask)
{
    if (getClientMajorVersion() <= 3 && getClientMinorVersion() < 1 &&
        EnsureGLExtensionEnabled("GL_ANGLE_texture_multisample"))
    {
        glSampleMaskiANGLE(maskNumber, mask);
    }
    else
    {
        glSampleMaski(maskNumber, mask);
    }
}

// Tests that if es version < 3.1, GL_TEXTURE_2D_MULTISAMPLE is not supported in
// GetInternalformativ. Checks that the number of samples returned is valid in case of ES >= 3.1.
TEST_P(TextureMultisampleTest, MultisampleTargetGetInternalFormativBase)
{
    ANGLE_SKIP_TEST_IF(lessThanES31MultisampleExtNotSupported());

    // This query returns supported sample counts in descending order. If only one sample count is
    // queried, it should be the maximum one.
    GLint maxSamplesR8 = 0;
    glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE, GL_R8, GL_SAMPLES, 1, &maxSamplesR8);

    // GLES 3.1 section 19.3.1 specifies the required minimum of how many samples are supported.
    GLint maxColorTextureSamples;
    glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &maxColorTextureSamples);
    GLint maxSamples;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    GLint maxSamplesR8Required = std::min(maxColorTextureSamples, maxSamples);

    EXPECT_GE(maxSamplesR8, maxSamplesR8Required);
    ASSERT_GL_NO_ERROR();
}

// Tests that if es version < 3.1 and multisample extension is unsupported,
// GL_TEXTURE_2D_MULTISAMPLE_ANGLE is not supported in FramebufferTexture2D.
TEST_P(TextureMultisampleTest, MultisampleTargetFramebufferTexture2D)
{
    ANGLE_SKIP_TEST_IF(lessThanES31MultisampleExtNotSupported());
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(IsVulkan());
    GLint samples = 1;
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mTexture);
    texStorageMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA8, 64, 64, GL_FALSE);

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                           mTexture, 0);

    ASSERT_GL_NO_ERROR();
}

// Tests basic functionality of glTexStorage2DMultisample.
TEST_P(TextureMultisampleTest, ValidateTextureStorageMultisampleParameters)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(IsVulkan());
    ANGLE_SKIP_TEST_IF(lessThanES31MultisampleExtNotSupported());

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mTexture);
    texStorageMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, 1, 1, GL_FALSE);
    ASSERT_GL_NO_ERROR();

    GLint params = 0;
    glGetTexParameteriv(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_IMMUTABLE_FORMAT, &params);
    EXPECT_EQ(1, params);

    texStorageMultisample(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1, GL_FALSE);
    ASSERT_GL_ERROR(GL_INVALID_ENUM);

    texStorageMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, 0, 0, GL_FALSE);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    texStorageMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, maxSize + 1, 1, GL_FALSE);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    GLint maxSamples = 0;
    glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE, GL_R8, GL_SAMPLES, 1, &maxSamples);
    texStorageMultisample(GL_TEXTURE_2D_MULTISAMPLE, maxSamples + 1, GL_RGBA8, 1, 1, GL_FALSE);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    texStorageMultisample(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_RGBA8, 1, 1, GL_FALSE);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    texStorageMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA, 0, 0, GL_FALSE);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    texStorageMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, 1, 1, GL_FALSE);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
}

// Tests the value of MAX_INTEGER_SAMPLES is no less than 1.
// [OpenGL ES 3.1 SPEC Table 20.40]
TEST_P(TextureMultisampleTest, MaxIntegerSamples)
{
    ANGLE_SKIP_TEST_IF(lessThanES31MultisampleExtNotSupported());
    GLint maxIntegerSamples;
    glGetIntegerv(GL_MAX_INTEGER_SAMPLES, &maxIntegerSamples);
    EXPECT_GE(maxIntegerSamples, 1);
    EXPECT_NE(std::numeric_limits<GLint>::max(), maxIntegerSamples);
}

// Tests the value of MAX_COLOR_TEXTURE_SAMPLES is no less than 1.
// [OpenGL ES 3.1 SPEC Table 20.40]
TEST_P(TextureMultisampleTest, MaxColorTextureSamples)
{
    ANGLE_SKIP_TEST_IF(lessThanES31MultisampleExtNotSupported());
    GLint maxColorTextureSamples;
    glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &maxColorTextureSamples);
    EXPECT_GE(maxColorTextureSamples, 1);
    EXPECT_NE(std::numeric_limits<GLint>::max(), maxColorTextureSamples);
}

// Tests the value of MAX_DEPTH_TEXTURE_SAMPLES is no less than 1.
// [OpenGL ES 3.1 SPEC Table 20.40]
TEST_P(TextureMultisampleTest, MaxDepthTextureSamples)
{
    ANGLE_SKIP_TEST_IF(lessThanES31MultisampleExtNotSupported());
    GLint maxDepthTextureSamples;
    glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &maxDepthTextureSamples);
    EXPECT_GE(maxDepthTextureSamples, 1);
    EXPECT_NE(std::numeric_limits<GLint>::max(), maxDepthTextureSamples);
}

// Tests that getTexLevelParameter is supported by ES 3.1 or ES 3.0 and ANGLE_texture_multisample
TEST_P(TextureMultisampleTest, GetTexLevelParameter)
{
    ANGLE_SKIP_TEST_IF(lessThanES31MultisampleExtNotSupported());
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(IsVulkan());

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mTexture);
    texStorageMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, 1, 1, GL_TRUE);
    ASSERT_GL_NO_ERROR();

    GLfloat levelSamples = 0;
    getTexLevelParameterfv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_SAMPLES, &levelSamples);
    EXPECT_EQ(levelSamples, 4);

    GLint fixedSampleLocation = false;
    getTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_FIXED_SAMPLE_LOCATIONS,
                           &fixedSampleLocation);
    EXPECT_EQ(fixedSampleLocation, 1);

    ASSERT_GL_NO_ERROR();
}

// The value of sample position should be equal to standard pattern on D3D.
TEST_P(TextureMultisampleTest, CheckSamplePositions)
{
    ANGLE_SKIP_TEST_IF(!IsD3D11());

    GLsizei maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);

    GLfloat samplePosition[2];

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFramebuffer);

    for (int sampleCount = 1; sampleCount <= maxSamples; sampleCount++)
    {
        GLTexture texture;
        size_t indexKey = static_cast<size_t>(ceil(log2(sampleCount)));
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texture);
        texStorageMultisample(GL_TEXTURE_2D_MULTISAMPLE, sampleCount, GL_RGBA8, 1, 1, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                               texture, 0);
        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        ASSERT_GL_NO_ERROR();

        for (int sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
        {
            getMultisamplefv(GL_SAMPLE_POSITION, sampleIndex, samplePosition);
            EXPECT_EQ(samplePosition[0], kSamplePositions[indexKey][2 * sampleIndex]);
            EXPECT_EQ(samplePosition[1], kSamplePositions[indexKey][2 * sampleIndex + 1]);
        }
    }

    ASSERT_GL_NO_ERROR();
}

// Test textureSize and texelFetch when using ANGLE_texture_multisample extension
TEST_P(TextureMultisampleTest, SimpleTexelFetch)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_texture_multisample"));

    ANGLE_GL_PROGRAM(texelFetchProgram, essl3_shaders::vs::Passthrough(),
                     multisampleTextureFragmentShader());

    GLint texLocation = glGetUniformLocation(texelFetchProgram, "tex");
    ASSERT_GE(texLocation, 0);
    GLint sampleNumLocation = glGetUniformLocation(texelFetchProgram, "sampleNum");
    ASSERT_GE(sampleNumLocation, 0);

    const GLsizei kWidth  = 4;
    const GLsizei kHeight = 4;

    std::vector<GLenum> testFormats = {GL_RGBA8};
    GLint samplesToUse              = getSamplesToUse(GL_TEXTURE_2D_MULTISAMPLE_ANGLE, testFormats);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ANGLE, mTexture);
    texStorageMultisample(GL_TEXTURE_2D_MULTISAMPLE_ANGLE, samplesToUse, GL_RGBA8, kWidth, kHeight,
                          GL_TRUE);
    ASSERT_GL_NO_ERROR();

    // Clear texture zero to green.
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    GLColor clearColor = GLColor::green;

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE_ANGLE,
                           mTexture, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, status);
    glClearColor(clearColor.R / 255.0f, clearColor.G / 255.0f, clearColor.B / 255.0f,
                 clearColor.A / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(texelFetchProgram);
    glViewport(0, 0, kWidth, kHeight);

    for (GLint sampleNum = 0; sampleNum < samplesToUse; ++sampleNum)
    {
        glUniform1i(sampleNumLocation, sampleNum);
        drawQuad(texelFetchProgram, essl31_shaders::PositionAttrib(), 0.5f, 1.0f, true);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_RECT_EQ(0, 0, kWidth, kHeight, clearColor);
    }
}

TEST_P(TextureMultisampleTest, SampleMaski)
{
    ANGLE_SKIP_TEST_IF(lessThanES31MultisampleExtNotSupported());

    GLint maxSampleMaskWords = 0;
    glGetIntegerv(GL_MAX_SAMPLE_MASK_WORDS, &maxSampleMaskWords);
    sampleMaski(maxSampleMaskWords - 1, 0x1);
    ASSERT_GL_NO_ERROR();

    glGetIntegerv(GL_MAX_SAMPLE_MASK_WORDS, &maxSampleMaskWords);
    sampleMaski(maxSampleMaskWords, 0x1);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);
}

// Negative tests of multisample texture. When context less than ES 3.1 and ANGLE_texture_multsample
// not enabled, the feature isn't supported.
TEST_P(NegativeTextureMultisampleTest, Negtive)
{
    ANGLE_SKIP_TEST_IF(EnsureGLExtensionEnabled("GL_ANGLE_texture_multisample"));

    GLint maxSamples = 0;
    glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE, GL_R8, GL_SAMPLES, 1, &maxSamples);
    ASSERT_GL_ERROR(GL_INVALID_ENUM);

    GLint maxColorTextureSamples;
    glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &maxColorTextureSamples);
    ASSERT_GL_ERROR(GL_INVALID_ENUM);

    GLint maxDepthTextureSamples;
    glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &maxDepthTextureSamples);
    ASSERT_GL_ERROR(GL_INVALID_ENUM);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mTexture);
    ASSERT_GL_ERROR(GL_INVALID_ENUM);

    texStorageMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, 64, 64, GL_FALSE);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                           mTexture, 0);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    GLint params = 0;
    glGetTexParameteriv(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_IMMUTABLE_FORMAT, &params);
    ASSERT_GL_ERROR(GL_INVALID_ENUM);

    GLfloat levelSamples = 0;
    getTexLevelParameterfv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_SAMPLES, &levelSamples);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    GLint fixedSampleLocation = false;
    getTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_FIXED_SAMPLE_LOCATIONS,
                           &fixedSampleLocation);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    GLint maxSampleMaskWords = 0;
    glGetIntegerv(GL_MAX_SAMPLE_MASK_WORDS, &maxSampleMaskWords);
    ASSERT_GL_ERROR(GL_INVALID_ENUM);
    sampleMaski(maxSampleMaskWords - 1, 0x1);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
}

// Tests that GL_TEXTURE_2D_MULTISAMPLE_ARRAY is not supported in GetInternalformativ when the
// extension is not supported.
TEST_P(TextureMultisampleArrayWebGLTest, MultisampleArrayTargetGetInternalFormativWithoutExtension)
{
    GLint maxSamples = 0;
    glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_RGBA8, GL_SAMPLES, 1,
                          &maxSamples);
    ASSERT_GL_ERROR(GL_INVALID_ENUM);
}

// Attempt to bind a texture to multisample array binding point when extension is not supported.
TEST_P(TextureMultisampleArrayWebGLTest, BindMultisampleArrayTextureWithoutExtension)
{
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, mTexture);
    ASSERT_GL_ERROR(GL_INVALID_ENUM);
}

// Try to compile shaders using GL_OES_texture_storage_multisample_2d_array when the extension is
// not enabled.
TEST_P(TextureMultisampleArrayWebGLTest, ShaderWithoutExtension)
{
    constexpr char kFSRequiresExtension[] = R"(#version 310 es
#extension GL_OES_texture_storage_multisample_2d_array : require
out highp vec4 my_FragColor;

void main() {
        my_FragColor = vec4(0.0);
})";

    GLuint program = CompileProgram(essl31_shaders::vs::Simple(), kFSRequiresExtension);
    EXPECT_EQ(0u, program);

    constexpr char kFSEnableAndUseExtension[] = R"(#version 310 es
#extension GL_OES_texture_storage_multisample_2d_array : enable

uniform highp sampler2DMSArray tex;
out highp ivec4 outSize;

void main() {
        outSize = ivec4(textureSize(tex), 0);
})";

    program = CompileProgram(essl31_shaders::vs::Simple(), kFSEnableAndUseExtension);
    EXPECT_EQ(0u, program);
}

// Tests that GL_TEXTURE_2D_MULTISAMPLE_ARRAY is supported in GetInternalformativ.
TEST_P(TextureMultisampleArrayWebGLTest, MultisampleArrayTargetGetInternalFormativ)
{
    ANGLE_SKIP_TEST_IF(!requestArrayExtension());

    // This query returns supported sample counts in descending order. If only one sample count is
    // queried, it should be the maximum one.
    GLint maxSamplesRGBA8 = 0;
    glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_RGBA8, GL_SAMPLES, 1,
                          &maxSamplesRGBA8);
    GLint maxSamplesDepth = 0;
    glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_DEPTH_COMPONENT24, GL_SAMPLES, 1,
                          &maxSamplesDepth);
    ASSERT_GL_NO_ERROR();

    // GLES 3.1 section 19.3.1 specifies the required minimum of how many samples are supported.
    GLint maxColorTextureSamples;
    glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &maxColorTextureSamples);
    GLint maxDepthTextureSamples;
    glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &maxDepthTextureSamples);
    GLint maxSamples;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);

    GLint maxSamplesRGBA8Required = std::min(maxColorTextureSamples, maxSamples);
    EXPECT_GE(maxSamplesRGBA8, maxSamplesRGBA8Required);

    GLint maxSamplesDepthRequired = std::min(maxDepthTextureSamples, maxSamples);
    EXPECT_GE(maxSamplesDepth, maxSamplesDepthRequired);
}

// Tests that TexImage3D call cannot be used for GL_TEXTURE_2D_MULTISAMPLE_ARRAY.
TEST_P(TextureMultisampleArrayWebGLTest, MultiSampleArrayTexImage)
{
    ANGLE_SKIP_TEST_IF(!requestArrayExtension());

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, mTexture);
    ASSERT_GL_NO_ERROR();

    glTexImage3D(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, 0, GL_RGBA8, 1, 1, 1, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Tests passing invalid parameters to TexStorage3DMultisample.
TEST_P(TextureMultisampleArrayWebGLTest, InvalidTexStorage3DMultisample)
{
    ANGLE_SKIP_TEST_IF(!requestArrayExtension());

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, mTexture);
    ASSERT_GL_NO_ERROR();

    // Invalid target
    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE, 2, GL_RGBA8, 1, 1, 1, GL_TRUE);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Samples 0
    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, 0, GL_RGBA8, 1, 1, 1,
                                 GL_TRUE);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Unsized internalformat
    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, 2, GL_RGBA, 1, 1, 1, GL_TRUE);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Width 0
    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, 2, GL_RGBA8, 0, 1, 1,
                                 GL_TRUE);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Height 0
    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, 2, GL_RGBA8, 1, 0, 1,
                                 GL_TRUE);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Depth 0
    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, 2, GL_RGBA8, 1, 1, 0,
                                 GL_TRUE);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Tests passing invalid parameters to TexParameteri.
TEST_P(TextureMultisampleArrayWebGLTest, InvalidTexParameteri)
{
    ANGLE_SKIP_TEST_IF(!requestArrayExtension());

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, mTexture);
    ASSERT_GL_NO_ERROR();

    // None of the sampler parameters can be set on GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES.
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_TEXTURE_MIN_LOD, 0);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_TEXTURE_MAX_LOD, 0);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_TEXTURE_COMPARE_FUNC, GL_ALWAYS);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Only valid base level on GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES is 0.
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_TEXTURE_BASE_LEVEL, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test a valid TexStorage3DMultisample call and check that the queried texture level parameters
// match. Does not do any drawing.
TEST_P(TextureMultisampleArrayWebGLTest, TexStorage3DMultisample)
{
    ANGLE_SKIP_TEST_IF(!requestArrayExtension());

    GLint maxSamplesRGBA8 = 0;
    glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_RGBA8, GL_SAMPLES, 1,
                          &maxSamplesRGBA8);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, mTexture);
    ASSERT_GL_NO_ERROR();

    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, maxSamplesRGBA8, GL_RGBA8, 8,
                                 4, 2, GL_TRUE);
    ASSERT_GL_NO_ERROR();

    GLint width = 0, height = 0, depth = 0, samples = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, 0, GL_TEXTURE_HEIGHT, &height);
    glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, 0, GL_TEXTURE_DEPTH, &depth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, 0, GL_TEXTURE_SAMPLES, &samples);
    ASSERT_GL_NO_ERROR();

    EXPECT_EQ(8, width);
    EXPECT_EQ(4, height);
    EXPECT_EQ(2, depth);
    EXPECT_EQ(maxSamplesRGBA8, samples);
}

// Test for invalid FramebufferTextureLayer calls with GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES
// textures.
TEST_P(TextureMultisampleArrayWebGLTest, InvalidFramebufferTextureLayer)
{
    ANGLE_SKIP_TEST_IF(!requestArrayExtension());

    GLint maxSamplesRGBA8 = 0;
    glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, GL_RGBA8, GL_SAMPLES, 1,
                          &maxSamplesRGBA8);

    GLint maxArrayTextureLayers;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayTextureLayers);

    // Test framebuffer status with just a color texture attached.
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, mTexture);
    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, maxSamplesRGBA8, GL_RGBA8, 4,
                                 4, 2, GL_TRUE);
    ASSERT_GL_NO_ERROR();

    // Test with mip level 1 and -1 (only level 0 is valid for multisample textures).
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTexture, 1, 0);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTexture, -1, 0);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Test with layer -1 and layer == MAX_ARRAY_TEXTURE_LAYERS
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTexture, 0, -1);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTexture, 0,
                              maxArrayTextureLayers);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Attach layers of TEXTURE_2D_MULTISAMPLE_ARRAY textures to a framebuffer and check for
// completeness.
TEST_P(TextureMultisampleArrayWebGLTest, FramebufferCompleteness)
{
    ANGLE_SKIP_TEST_IF(!requestArrayExtension());

    std::vector<GLenum> testFormats = {{GL_RGBA8, GL_DEPTH_COMPONENT24, GL_DEPTH24_STENCIL8}};
    GLint samplesToUse = getSamplesToUse(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, testFormats);

    // Test framebuffer status with just a color texture attached.
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, mTexture);
    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, samplesToUse, GL_RGBA8, 4, 4,
                                 2, GL_TRUE);
    ASSERT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTexture, 0, 0);
    ASSERT_GL_NO_ERROR();

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, status);

    // Test framebuffer status with both color and depth textures attached.
    GLTexture depthTexture;
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, depthTexture);
    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, samplesToUse,
                                 GL_DEPTH_COMPONENT24, 4, 4, 2, GL_TRUE);
    ASSERT_GL_NO_ERROR();

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture, 0, 0);
    ASSERT_GL_NO_ERROR();

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, status);

    // Test with color and depth/stencil textures attached.
    GLTexture depthStencilTexture;
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, depthStencilTexture);
    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, samplesToUse,
                                 GL_DEPTH24_STENCIL8, 4, 4, 2, GL_TRUE);
    ASSERT_GL_NO_ERROR();

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, depthStencilTexture, 0,
                              0);
    ASSERT_GL_NO_ERROR();

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, status);
}

// Attach a layer of TEXTURE_2D_MULTISAMPLE_ARRAY texture to a framebuffer, clear it, and resolve by
// blitting.
TEST_P(TextureMultisampleArrayWebGLTest, FramebufferColorClearAndBlit)
{
    ANGLE_SKIP_TEST_IF(!requestArrayExtension());

    const GLsizei kWidth  = 4;
    const GLsizei kHeight = 4;

    std::vector<GLenum> testFormats = {GL_RGBA8};
    GLint samplesToUse = getSamplesToUse(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, testFormats);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, mTexture);
    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, samplesToUse, GL_RGBA8,
                                 kWidth, kHeight, 2, GL_TRUE);

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTexture, 0, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    ASSERT_GL_NO_ERROR();
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, status);

    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLFramebuffer resolveFramebuffer;
    GLTexture resolveTexture;
    glBindTexture(GL_TEXTURE_2D, resolveTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kWidth, kHeight);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFramebuffer);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolveTexture,
                           0);
    glBlitFramebuffer(0, 0, kWidth, kHeight, 0, 0, kWidth, kHeight, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFramebuffer);
    EXPECT_PIXEL_RECT_EQ(0, 0, kWidth, kHeight, GLColor::green);
}

// Check the size of a multisample array texture in a shader.
TEST_P(TextureMultisampleArrayWebGLTest, TextureSizeInShader)
{
    ANGLE_SKIP_TEST_IF(!requestArrayExtension());

    constexpr char kFS[] = R"(#version 310 es
#extension GL_OES_texture_storage_multisample_2d_array : require

uniform highp sampler2DMSArray tex;
out highp vec4 my_FragColor;

void main() {
        my_FragColor = (textureSize(tex) == ivec3(8, 4, 2)) ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);
})";

    ANGLE_GL_PROGRAM(texSizeProgram, essl31_shaders::vs::Simple(), kFS);

    GLint texLocation = glGetUniformLocation(texSizeProgram, "tex");
    ASSERT_GE(texLocation, 0);

    const GLsizei kWidth  = 8;
    const GLsizei kHeight = 4;

    std::vector<GLenum> testFormats = {GL_RGBA8};
    GLint samplesToUse = getSamplesToUse(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, testFormats);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, mTexture);
    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, samplesToUse, GL_RGBA8,
                                 kWidth, kHeight, 2, GL_TRUE);
    ASSERT_GL_NO_ERROR();

    drawQuad(texSizeProgram, essl31_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Clear the layers of a multisample array texture, and then sample all the samples from all the
// layers in a shader.
TEST_P(TextureMultisampleArrayWebGLTest, SimpleTexelFetch)
{
    ANGLE_SKIP_TEST_IF(!requestArrayExtension());

    ANGLE_GL_PROGRAM(texelFetchProgram, essl31_shaders::vs::Passthrough(),
                     blitArrayTextureLayerFragmentShader());

    GLint texLocation = glGetUniformLocation(texelFetchProgram, "tex");
    ASSERT_GE(texLocation, 0);
    GLint layerLocation = glGetUniformLocation(texelFetchProgram, "layer");
    ASSERT_GE(layerLocation, 0);
    GLint sampleNumLocation = glGetUniformLocation(texelFetchProgram, "sampleNum");
    ASSERT_GE(layerLocation, 0);

    const GLsizei kWidth      = 4;
    const GLsizei kHeight     = 4;
    const GLsizei kLayerCount = 2;

    std::vector<GLenum> testFormats = {GL_RGBA8};
    GLint samplesToUse = getSamplesToUse(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, testFormats);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, mTexture);
    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, samplesToUse, GL_RGBA8,
                                 kWidth, kHeight, kLayerCount, GL_TRUE);
    ASSERT_GL_NO_ERROR();

    // Clear layer zero to green and layer one to blue.
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    std::vector<GLColor> clearColors = {{GLColor::green, GLColor::blue}};
    for (GLint i = 0; static_cast<GLsizei>(i) < kLayerCount; ++i)
    {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTexture, 0, i);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, status);
        const GLColor &clearColor = clearColors[i];
        glClearColor(clearColor.R / 255.0f, clearColor.G / 255.0f, clearColor.B / 255.0f,
                     clearColor.A / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ASSERT_GL_NO_ERROR();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(texelFetchProgram);
    glViewport(0, 0, kWidth, kHeight);
    for (GLint layer = 0; static_cast<GLsizei>(layer) < kLayerCount; ++layer)
    {
        glUniform1i(layerLocation, layer);
        for (GLint sampleNum = 0; sampleNum < samplesToUse; ++sampleNum)
        {
            glUniform1i(sampleNumLocation, sampleNum);
            drawQuad(texelFetchProgram, essl31_shaders::PositionAttrib(), 0.5f, 1.0f, true);
            ASSERT_GL_NO_ERROR();
            EXPECT_PIXEL_RECT_EQ(0, 0, kWidth, kHeight, clearColors[layer]);
        }
    }
}

// Clear the layers of an integer multisample array texture, and then sample all the samples from
// all the layers in a shader.
TEST_P(TextureMultisampleArrayWebGLTest, IntegerTexelFetch)
{
    ANGLE_SKIP_TEST_IF(!requestArrayExtension());

    ANGLE_GL_PROGRAM(texelFetchProgram, essl31_shaders::vs::Passthrough(),
                     blitIntArrayTextureLayerFragmentShader());

    GLint texLocation = glGetUniformLocation(texelFetchProgram, "tex");
    ASSERT_GE(texLocation, 0);
    GLint layerLocation = glGetUniformLocation(texelFetchProgram, "layer");
    ASSERT_GE(layerLocation, 0);
    GLint sampleNumLocation = glGetUniformLocation(texelFetchProgram, "sampleNum");
    ASSERT_GE(layerLocation, 0);

    const GLsizei kWidth      = 4;
    const GLsizei kHeight     = 4;
    const GLsizei kLayerCount = 2;

    std::vector<GLenum> testFormats = {GL_RGBA8I};
    GLint samplesToUse = getSamplesToUse(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, testFormats);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, mTexture);
    glTexStorage3DMultisampleOES(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES, samplesToUse, GL_RGBA8I,
                                 kWidth, kHeight, kLayerCount, GL_TRUE);
    ASSERT_GL_NO_ERROR();

    // Clear layer zero to green and layer one to blue.
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    std::vector<GLColor> clearColors = {{GLColor::green, GLColor::blue}};
    for (GLint i = 0; static_cast<GLsizei>(i) < kLayerCount; ++i)
    {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTexture, 0, i);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, status);
        std::array<GLint, 4> intColor;
        for (size_t j = 0; j < intColor.size(); ++j)
        {
            intColor[j] = clearColors[i][j] / 255;
        }
        glClearBufferiv(GL_COLOR, 0, intColor.data());
        ASSERT_GL_NO_ERROR();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(texelFetchProgram);
    glViewport(0, 0, kWidth, kHeight);
    for (GLint layer = 0; static_cast<GLsizei>(layer) < kLayerCount; ++layer)
    {
        glUniform1i(layerLocation, layer);
        for (GLint sampleNum = 0; sampleNum < samplesToUse; ++sampleNum)
        {
            glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glUniform1i(sampleNumLocation, sampleNum);
            drawQuad(texelFetchProgram, essl31_shaders::PositionAttrib(), 0.5f, 1.0f, true);
            ASSERT_GL_NO_ERROR();
            EXPECT_PIXEL_RECT_EQ(0, 0, kWidth, kHeight, clearColors[layer]);
        }
    }
}

ANGLE_INSTANTIATE_TEST_ES3_AND_ES31(TextureMultisampleTest);
ANGLE_INSTANTIATE_TEST_ES3(NegativeTextureMultisampleTest);
ANGLE_INSTANTIATE_TEST_ES31(TextureMultisampleArrayWebGLTest);
}  // anonymous namespace
