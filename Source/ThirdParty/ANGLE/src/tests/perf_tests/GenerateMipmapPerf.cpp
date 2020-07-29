//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GenerateMipmapBenchmark:
//   Performance test for generating texture mipmaps.
//

#include "ANGLEPerfTest.h"

#include <iostream>
#include <random>
#include <sstream>

#include "test_utils/gl_raii.h"
#include "util/shader_utils.h"

using namespace angle;

namespace
{
constexpr unsigned int kIterationsPerStep = 5;

struct GenerateMipmapParams final : public RenderTestParams
{
    GenerateMipmapParams()
    {
        iterationsPerStep = kIterationsPerStep;
        trackGpuTime      = true;

        textureWidth  = 1920;
        textureHeight = 1080;

        internalFormat = GL_RGBA;

        webgl = false;
    }

    std::string story() const override;

    GLsizei textureWidth;
    GLsizei textureHeight;

    GLenum internalFormat;

    bool webgl;
};

std::ostream &operator<<(std::ostream &os, const GenerateMipmapParams &params)
{
    return os << params.backendAndStory().substr(1);
}

std::string GenerateMipmapParams::story() const
{
    std::stringstream strstr;

    strstr << RenderTestParams::story();

    if (webgl)
    {
        strstr << "_webgl";
    }

    if (internalFormat == GL_RGB)
    {
        strstr << "_rgb";
    }

    return strstr.str();
}

template <typename T>
void FillWithRandomData(T *storage)
{
    for (uint8_t &u : *storage)
    {
        u = rand() & 0xFF;
    }
}

class GenerateMipmapBenchmarkBase : public ANGLERenderTest,
                                    public ::testing::WithParamInterface<GenerateMipmapParams>
{
  public:
    GenerateMipmapBenchmarkBase(const char *benchmarkName);

    void initializeBenchmark() override;
    void destroyBenchmark() override;

  protected:
    void initShaders();

    GLuint mProgram    = 0;
    GLint mPositionLoc = -1;
    GLint mSamplerLoc  = -1;
    GLuint mTexture    = 0;

    std::vector<uint8_t> mTextureData;
};

class GenerateMipmapBenchmark : public GenerateMipmapBenchmarkBase
{
  public:
    GenerateMipmapBenchmark() : GenerateMipmapBenchmarkBase("GenerateMipmap") {}

    void initializeBenchmark() override;

    void drawBenchmark() override;
};

class GenerateMipmapWithRedefineBenchmark : public GenerateMipmapBenchmarkBase
{
  public:
    GenerateMipmapWithRedefineBenchmark()
        : GenerateMipmapBenchmarkBase("GenerateMipmapWithRedefine")
    {}

    void drawBenchmark() override;
};

GenerateMipmapBenchmarkBase::GenerateMipmapBenchmarkBase(const char *benchmarkName)
    : ANGLERenderTest(benchmarkName, GetParam())
{
    setWebGLCompatibilityEnabled(GetParam().webgl);
    setRobustResourceInit(GetParam().webgl);

    // Crashes on nvidia+d3d11. http://crbug.com/945415
    if (GetParam().getRenderer() == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        mSkipTest = true;
    }

    // Fails on Windows7 NVIDIA Vulkan, presumably due to old drivers. http://crbug.com/1096510
    if (IsWindows7() && IsNVIDIA() &&
        GetParam().eglParameters.renderer == EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE)
    {
        mSkipTest = true;
    }
}

void GenerateMipmapBenchmarkBase::initializeBenchmark()
{
    const auto &params = GetParam();

    initShaders();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glViewport(0, 0, getWindow()->getWidth(), getWindow()->getHeight());

    if (params.webgl)
    {
        glRequestExtensionANGLE("GL_EXT_disjoint_timer_query");
    }

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_2D, mTexture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    mTextureData.resize(params.textureWidth * params.textureHeight * 4);
    FillWithRandomData(&mTextureData);

    glTexImage2D(GL_TEXTURE_2D, 0, params.internalFormat, params.textureWidth, params.textureHeight,
                 0, params.internalFormat, GL_UNSIGNED_BYTE, mTextureData.data());

    // Perform a draw so the image data is flushed.
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    ASSERT_GL_NO_ERROR();
}

void GenerateMipmapBenchmarkBase::initShaders()
{
    constexpr char kVS[] = R"(attribute vec4 a_position;
void main()
{
    gl_Position = a_position;
})";

    constexpr char kFS[] = R"(precision mediump float;
uniform sampler2D s_texture;
void main()
{
    gl_FragColor = texture2D(s_texture, vec2(0, 0));
})";

    mProgram = CompileProgram(kVS, kFS);
    ASSERT_NE(0u, mProgram);

    mPositionLoc = glGetAttribLocation(mProgram, "a_position");
    mSamplerLoc  = glGetUniformLocation(mProgram, "s_texture");
    glUseProgram(mProgram);
    glUniform1i(mSamplerLoc, 0);

    glDisable(GL_DEPTH_TEST);

    ASSERT_GL_NO_ERROR();
}

void GenerateMipmapBenchmarkBase::destroyBenchmark()
{
    glDeleteTextures(1, &mTexture);
    glDeleteProgram(mProgram);
}

void GenerateMipmapBenchmark::initializeBenchmark()
{
    GenerateMipmapBenchmarkBase::initializeBenchmark();

    // Generate mipmaps once so the texture doesn't need to be redefined.
    glGenerateMipmap(GL_TEXTURE_2D);

    // Perform a draw so the image data is flushed.
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void GenerateMipmapBenchmark::drawBenchmark()
{
    const auto &params = GetParam();

    startGpuTimer();
    for (unsigned int iteration = 0; iteration < params.iterationsPerStep; ++iteration)
    {
        // Slightly modify the base texture so the mipmap is definitely regenerated.
        std::array<uint8_t, 4> randomData;
        FillWithRandomData(&randomData);

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, params.internalFormat, GL_UNSIGNED_BYTE,
                        randomData.data());

        // Generate mipmaps
        glGenerateMipmap(GL_TEXTURE_2D);

        // Perform a draw just so the texture data is flushed.  With the position attributes not
        // set, a constant default value is used, resulting in a very cheap draw.
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    stopGpuTimer();

    ASSERT_GL_NO_ERROR();
}

void GenerateMipmapWithRedefineBenchmark::drawBenchmark()
{
    const auto &params = GetParam();

    // Create a new texture every time, so image redefinition happens every time.
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, params.internalFormat, params.textureWidth, params.textureHeight,
                 0, params.internalFormat, GL_UNSIGNED_BYTE, mTextureData.data());

    // Perform a draw so the image data is flushed.
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    startGpuTimer();

    // Do a single iteration, otherwise the cost of redefinition is amortized.
    ASSERT_EQ(params.iterationsPerStep, 1u);

    // Generate mipmaps
    glGenerateMipmap(GL_TEXTURE_2D);

    // Perform a draw just so the texture data is flushed.  With the position attributes not
    // set, a constant default value is used, resulting in a very cheap draw.
    glDrawArrays(GL_TRIANGLES, 0, 3);

    stopGpuTimer();

    ASSERT_GL_NO_ERROR();
}

GenerateMipmapParams D3D11Params(bool webglCompat, bool singleIteration)
{
    GenerateMipmapParams params;
    params.eglParameters = egl_platform::D3D11();
    params.majorVersion  = 3;
    params.minorVersion  = 0;
    params.webgl         = webglCompat;
    if (singleIteration)
    {
        params.iterationsPerStep = 1;
    }
    return params;
}

GenerateMipmapParams OpenGLOrGLESParams(bool webglCompat, bool singleIteration)
{
    GenerateMipmapParams params;
    params.eglParameters = egl_platform::OPENGL_OR_GLES();
    params.majorVersion  = 3;
    params.minorVersion  = 0;
    params.webgl         = webglCompat;
    if (singleIteration)
    {
        params.iterationsPerStep = 1;
    }
    return params;
}

GenerateMipmapParams VulkanParams(bool webglCompat, bool singleIteration, bool emulatedFormat)
{
    GenerateMipmapParams params;
    params.eglParameters = egl_platform::VULKAN();
    params.majorVersion  = 3;
    params.minorVersion  = 0;
    params.webgl         = webglCompat;
    if (emulatedFormat)
    {
        params.internalFormat = GL_RGB;
    }
    if (singleIteration)
    {
        params.iterationsPerStep = 1;
    }
    return params;
}

}  // anonymous namespace

TEST_P(GenerateMipmapBenchmark, Run)
{
    run();
}

TEST_P(GenerateMipmapWithRedefineBenchmark, Run)
{
    run();
}

using namespace params;

ANGLE_INSTANTIATE_TEST(GenerateMipmapBenchmark,
                       D3D11Params(false, false),
                       D3D11Params(true, false),
                       OpenGLOrGLESParams(false, false),
                       OpenGLOrGLESParams(true, false),
                       VulkanParams(false, false, false),
                       VulkanParams(true, false, false),
                       VulkanParams(false, false, true),
                       VulkanParams(true, false, true));

ANGLE_INSTANTIATE_TEST(GenerateMipmapWithRedefineBenchmark,
                       D3D11Params(false, true),
                       D3D11Params(true, true),
                       OpenGLOrGLESParams(false, true),
                       OpenGLOrGLESParams(true, true),
                       VulkanParams(false, true, false),
                       VulkanParams(true, true, false),
                       VulkanParams(false, true, true),
                       VulkanParams(true, true, true));
