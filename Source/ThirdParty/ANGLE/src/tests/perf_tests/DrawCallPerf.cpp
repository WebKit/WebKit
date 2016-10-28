//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DrawCallPerf:
//   Performance tests for ANGLE draw call overhead.
//

#include <sstream>

#include "ANGLEPerfTest.h"
#include "shader_utils.h"

using namespace angle;

namespace
{

struct DrawCallPerfParams final : public RenderTestParams
{
    // Common default options
    DrawCallPerfParams()
    {
        majorVersion = 2;
        minorVersion = 0;
        windowWidth = 256;
        windowHeight = 256;
    }

    std::string suffix() const override
    {
        std::stringstream strstr;

        strstr << RenderTestParams::suffix();

        if (numTris == 0)
        {
            strstr << "_validation_only";
        }

        if (useFBO)
        {
            strstr << "_render_to_texture";
        }

        if (eglParameters.deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE)
        {
            strstr << "_null";
        }

        return strstr.str();
    }

    unsigned int iterations = 50;
    double runTimeSeconds   = 10.0;
    int numTris             = 1;
    bool useFBO             = false;
};

std::ostream &operator<<(std::ostream &os, const DrawCallPerfParams &params)
{
    os << params.suffix().substr(1);
    return os;
}

class DrawCallPerfBenchmark : public ANGLERenderTest,
                              public ::testing::WithParamInterface<DrawCallPerfParams>
{
  public:
    DrawCallPerfBenchmark();

    void initializeBenchmark() override;
    void destroyBenchmark() override;
    void drawBenchmark() override;

  private:
    GLuint mProgram = 0;
    GLuint mBuffer  = 0;
    GLuint mFBO     = 0;
    GLuint mTexture = 0;
    int mNumTris    = GetParam().numTris;
};

DrawCallPerfBenchmark::DrawCallPerfBenchmark() : ANGLERenderTest("DrawCallPerf", GetParam())
{
    mRunTimeSeconds = GetParam().runTimeSeconds;
}

void DrawCallPerfBenchmark::initializeBenchmark()
{
    const auto &params = GetParam();

    ASSERT_LT(0u, params.iterations);

    const std::string vs = SHADER_SOURCE
    (
        attribute vec2 vPosition;
        uniform float uScale;
        uniform float uOffset;
        void main()
        {
            gl_Position = vec4(vPosition * vec2(uScale) - vec2(uOffset), 0, 1);
        }
    );

    const std::string fs = SHADER_SOURCE
    (
        precision mediump float;
        void main()
        {
            gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
    );

    mProgram = CompileProgram(vs, fs);
    ASSERT_NE(0u, mProgram);

    // Use the program object
    glUseProgram(mProgram);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    std::vector<GLfloat> floatData;

    for (int quadIndex = 0; quadIndex < mNumTris; ++quadIndex)
    {
        floatData.push_back(1);
        floatData.push_back(2);
        floatData.push_back(0);
        floatData.push_back(0);
        floatData.push_back(2);
        floatData.push_back(0);
    }

    glGenBuffers(1, &mBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, mBuffer);

    // To avoid generating GL errors when testing validation-only
    if (floatData.empty())
    {
        floatData.push_back(0.0f);
    }

    glBufferData(GL_ARRAY_BUFFER, floatData.size() * sizeof(GLfloat), &floatData[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    // Set the viewport
    glViewport(0, 0, getWindow()->getWidth(), getWindow()->getHeight());

    GLfloat scale = 0.5f;
    GLfloat offset = 0.5f;

    glUniform1f(glGetUniformLocation(mProgram, "uScale"), scale);
    glUniform1f(glGetUniformLocation(mProgram, "uOffset"), offset);

    if (params.useFBO)
    {
        glGenFramebuffers(1, &mFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        glGenTextures(1, &mTexture);
        glBindTexture(GL_TEXTURE_2D, mTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindow()->getWidth(), getWindow()->getHeight(),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture, 0);
    }

    ASSERT_GL_NO_ERROR();
}

void DrawCallPerfBenchmark::destroyBenchmark()
{
    glDeleteProgram(mProgram);
    glDeleteBuffers(1, &mBuffer);
    glDeleteTextures(1, &mTexture);
    glDeleteFramebuffers(1, &mFBO);
}

void DrawCallPerfBenchmark::drawBenchmark()
{
    // This workaround fixes a huge queue of graphics commands accumulating on the GL
    // back-end. The GL back-end doesn't have a proper NULL device at the moment.
    // TODO(jmadill): Remove this when/if we ever get a proper OpenGL NULL device.
    const auto &eglParams = GetParam().eglParameters;
    if (eglParams.deviceType != EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE ||
        (eglParams.renderer != EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE &&
         eglParams.renderer != EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE))
    {
        glClear(GL_COLOR_BUFFER_BIT);
    }

    const auto &params = GetParam();

    for (unsigned int it = 0; it < params.iterations; it++)
    {
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(3 * mNumTris));
    }

    ASSERT_GL_NO_ERROR();
}

using namespace egl_platform;

DrawCallPerfParams DrawCallPerfD3D11Params(bool useNullDevice, bool renderToTexture)
{
    DrawCallPerfParams params;
    params.eglParameters = useNullDevice ? D3D11_NULL() : D3D11();
    params.useFBO        = renderToTexture;
    return params;
}

DrawCallPerfParams DrawCallPerfD3D9Params(bool useNullDevice, bool renderToTexture)
{
    DrawCallPerfParams params;
    params.eglParameters = useNullDevice ? D3D9_NULL() : D3D9();
    params.useFBO        = renderToTexture;
    return params;
}

DrawCallPerfParams DrawCallPerfOpenGLParams(bool useNullDevice, bool renderToTexture)
{
    DrawCallPerfParams params;
    params.eglParameters = useNullDevice ? OPENGL_NULL() : OPENGL();
    params.useFBO        = renderToTexture;
    return params;
}

DrawCallPerfParams DrawCallPerfValidationOnly()
{
    DrawCallPerfParams params;
    params.eglParameters = DEFAULT();
    params.iterations     = 10000;
    params.numTris = 0;
    params.runTimeSeconds = 5.0;
    return params;
}

TEST_P(DrawCallPerfBenchmark, Run)
{
    run();
}

ANGLE_INSTANTIATE_TEST(DrawCallPerfBenchmark,
                       DrawCallPerfD3D9Params(false, false),
                       DrawCallPerfD3D9Params(true, false),
                       DrawCallPerfD3D11Params(false, false),
                       DrawCallPerfD3D11Params(true, false),
                       DrawCallPerfD3D11Params(true, true),
                       DrawCallPerfOpenGLParams(false, false),
                       DrawCallPerfOpenGLParams(true, false),
                       DrawCallPerfOpenGLParams(true, true),
                       DrawCallPerfValidationOnly());

} // namespace
