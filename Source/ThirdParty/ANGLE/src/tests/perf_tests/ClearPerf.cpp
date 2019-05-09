//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ClearPerf:
//   Performance test for clearing framebuffers.
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
constexpr unsigned int kIterationsPerStep = 256;

struct ClearParams final : public RenderTestParams
{
    ClearParams()
    {
        iterationsPerStep = kIterationsPerStep;
        trackGpuTime      = true;

        fboSize     = 2048;
        textureSize = 16;
    }

    std::string suffix() const override;

    GLsizei fboSize;
    GLsizei textureSize;
};

std::ostream &operator<<(std::ostream &os, const ClearParams &params)
{
    os << params.suffix().substr(1);
    return os;
}

std::string ClearParams::suffix() const
{
    std::stringstream strstr;

    strstr << RenderTestParams::suffix();

    return strstr.str();
}

class ClearBenchmark : public ANGLERenderTest, public ::testing::WithParamInterface<ClearParams>
{
  public:
    ClearBenchmark();

    void initializeBenchmark() override;
    void destroyBenchmark() override;
    void drawBenchmark() override;

  private:
    void initShaders();

    std::vector<GLuint> mTextures;

    GLuint mProgram;
    GLuint mPositionLoc;
    GLuint mSamplerLoc;
};

ClearBenchmark::ClearBenchmark()
    : ANGLERenderTest("Clear", GetParam()), mProgram(0u), mPositionLoc(-1), mSamplerLoc(-1)
{
    // Crashes on nvidia+d3d11. http://crbug.com/945415
    if (GetParam().getRenderer() == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        mSkipTest = true;
    }
}

void ClearBenchmark::initializeBenchmark()
{
    initShaders();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glViewport(0, 0, getWindow()->getWidth(), getWindow()->getHeight());

    ASSERT_GL_NO_ERROR();
}

void ClearBenchmark::initShaders()
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

    glDisable(GL_DEPTH_TEST);

    ASSERT_GL_NO_ERROR();
}

void ClearBenchmark::destroyBenchmark()
{
    glDeleteProgram(mProgram);
}

void ClearBenchmark::drawBenchmark()
{
    const auto &params = GetParam();

    std::vector<float> textureData(params.textureSize * params.textureSize * 4, 0.5);

    GLTexture tex;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, params.textureSize, params.textureSize, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, textureData.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glUniform1i(mSamplerLoc, 0);

    GLRenderbuffer colorRbo;
    glBindRenderbuffer(GL_RENDERBUFFER, colorRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, params.fboSize, params.fboSize);

    GLRenderbuffer depthRbo;
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, params.fboSize, params.fboSize);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);

    startGpuTimer();
    for (size_t it = 0; it < params.iterationsPerStep; ++it)
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    stopGpuTimer();

    ASSERT_GL_NO_ERROR();
}

ClearParams D3D11Params()
{
    ClearParams params;
    params.eglParameters = egl_platform::D3D11();
    return params;
}

ClearParams OpenGLOrGLESParams()
{
    ClearParams params;
    params.eglParameters = egl_platform::OPENGL_OR_GLES(false);
    return params;
}

ClearParams VulkanParams()
{
    ClearParams params;
    params.eglParameters = egl_platform::VULKAN();
    return params;
}

}  // anonymous namespace

TEST_P(ClearBenchmark, Run)
{
    run();
}

ANGLE_INSTANTIATE_TEST(ClearBenchmark, D3D11Params(), OpenGLOrGLESParams(), VulkanParams());
