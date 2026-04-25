//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FormatUploadDrawPerfBenchmark:
//   Performance of texture upload and draw using various formats.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "ANGLEPerfTest.h"

#include <iostream>
#include <random>
#include <sstream>

#include "test_utils/gl_raii.h"
#include "util/shader_utils.h"

using namespace angle;

enum class TestedFormat
{
    RGBA8,
    RGB8,
    RGB565,
};

constexpr const char *kTestedFormatString[] = {
    "rgba8",
    "rgb8",
    "rgb565",
};

template <typename E>
constexpr auto ToUnderlying(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

namespace
{
struct FormatUploadDrawPerfParams final : public RenderTestParams
{
    FormatUploadDrawPerfParams() { iterationsPerStep = 1; }

    std::string story() const override;

    TestedFormat testedFormat;
};

std::ostream &operator<<(std::ostream &os, const FormatUploadDrawPerfParams &params)
{
    return os << params.backendAndStory().substr(1);
}

std::string FormatUploadDrawPerfParams::story() const
{
    std::stringstream strstr;

    strstr << RenderTestParams::story() << "_" << kTestedFormatString[ToUnderlying(testedFormat)];

    return strstr.str();
}

class FormatUploadDrawPerfBenchmark
    : public ANGLERenderTest,
      public ::testing::WithParamInterface<FormatUploadDrawPerfParams>
{
  public:
    FormatUploadDrawPerfBenchmark();

    void initializeBenchmark() override;
    void drawBenchmark() override;
    void destroyBenchmark() override;

  protected:
    std::vector<uint8_t> mColors;
    GLuint mTexture;
    TestedFormat mTestedFormat;
    uint32_t mTextureSize;
    uint32_t mPixelSize;
    GLuint mFormat;
    GLuint mType;
    GLuint mProgram;
};

FormatUploadDrawPerfBenchmark::FormatUploadDrawPerfBenchmark()
    : ANGLERenderTest("FormatUploadDrawPerf", GetParam())
{
    mTestedFormat = GetParam().testedFormat;
    mTextureSize  = 256;
    switch (mTestedFormat)
    {
        case TestedFormat::RGBA8:
            mFormat    = GL_RGBA;
            mType      = GL_UNSIGNED_BYTE;
            mPixelSize = 4;
            break;
        case TestedFormat::RGB8:
            mFormat    = GL_RGB;
            mType      = GL_UNSIGNED_BYTE;
            mPixelSize = 3;
            break;
        case TestedFormat::RGB565:
            mFormat    = GL_RGB;
            mType      = GL_UNSIGNED_SHORT_5_6_5;
            mPixelSize = 2;
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void FormatUploadDrawPerfBenchmark::initializeBenchmark()
{
    // Initialize texture.
    ASSERT(gl::isPow2(mTextureSize));
    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, mFormat, mTextureSize, mTextureSize, 0, mFormat, mType, nullptr);

    // Initialize color data.
    mColors.resize(mTextureSize * mTextureSize * mPixelSize);
    memset(mColors.data(), 0, mTextureSize * mTextureSize * mPixelSize);

    // Set up program.
    std::string vs = R"(#version 300 es
out highp vec2 texcoord;
void main()
{
    gl_Position = vec4(0, 0, 0, 0);
    texcoord = vec2(0, 0);
}
)";
    std::string fs = R"(#version 300 es
uniform highp sampler2D tex;
out highp vec4 color;
in highp vec2 texcoord;
void main()
{
    color = texture(tex, texcoord);
})";

    mProgram = CompileProgram(vs.c_str(), fs.c_str());
    ASSERT_NE(0u, mProgram);
    glUseProgram(mProgram);
}

void FormatUploadDrawPerfBenchmark::destroyBenchmark()
{
    glDeleteTextures(1, &mTexture);
    glDeleteProgram(mProgram);
}

void FormatUploadDrawPerfBenchmark::drawBenchmark()
{
    // Upload and draw many times.
    for (uint32_t i = 0; i < 100; i++)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mTextureSize, mTextureSize, mFormat, mType,
                        mColors.data());
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    ASSERT_GL_NO_ERROR();
}

FormatUploadDrawPerfParams VulkanParams(TestedFormat testedFormat)
{
    FormatUploadDrawPerfParams params;
    params.eglParameters = egl_platform::VULKAN();
    params.majorVersion  = 3;
    params.minorVersion  = 0;
    params.testedFormat  = testedFormat;
    return params;
}

FormatUploadDrawPerfParams OpenGLOrGLESParams(TestedFormat testedFormat)
{
    FormatUploadDrawPerfParams params;
    params.eglParameters = egl_platform::OPENGL_OR_GLES();
    params.majorVersion  = 3;
    params.minorVersion  = 0;
    params.testedFormat  = testedFormat;
    return params;
}

FormatUploadDrawPerfParams MetalParams(TestedFormat testedFormat)
{
    FormatUploadDrawPerfParams params;
    params.eglParameters = egl_platform::METAL();
    params.majorVersion  = 3;
    params.minorVersion  = 0;
    params.testedFormat  = testedFormat;
    return params;
}

FormatUploadDrawPerfParams D3D11Params(TestedFormat testedFormat)
{
    FormatUploadDrawPerfParams params;
    params.eglParameters = egl_platform::D3D11();
    params.majorVersion  = 3;
    params.minorVersion  = 0;
    params.testedFormat  = testedFormat;
    return params;
}
}  // anonymous namespace

// Runs the test to measure the performance of various formats.
TEST_P(FormatUploadDrawPerfBenchmark, Run)
{
    run();
}

using namespace params;

ANGLE_INSTANTIATE_TEST(FormatUploadDrawPerfBenchmark,
                       VulkanParams(TestedFormat::RGBA8),
                       VulkanParams(TestedFormat::RGB8),
                       VulkanParams(TestedFormat::RGB565),
                       OpenGLOrGLESParams(TestedFormat::RGBA8),
                       OpenGLOrGLESParams(TestedFormat::RGB8),
                       OpenGLOrGLESParams(TestedFormat::RGB565),
                       MetalParams(TestedFormat::RGBA8),
                       MetalParams(TestedFormat::RGB8),
                       MetalParams(TestedFormat::RGB565),
                       D3D11Params(TestedFormat::RGBA8),
                       D3D11Params(TestedFormat::RGB8),
                       D3D11Params(TestedFormat::RGB565));
