//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UniformsBenchmark:
//   Performance test for setting uniform data.
//

#include "ANGLEPerfTest.h"

#include <iostream>
#include <random>
#include <sstream>

#include "shader_utils.h"

using namespace angle;

namespace
{

struct UniformsParams final : public RenderTestParams
{
    UniformsParams()
    {
        // Common default params
        majorVersion = 2;
        minorVersion = 0;
        windowWidth  = 720;
        windowHeight = 720;
        iterations   = 4;

        numVertexUniforms   = 200;
        numFragmentUniforms = 200;
    }

    std::string suffix() const override;
    size_t numVertexUniforms;
    size_t numFragmentUniforms;

    // static parameters
    size_t iterations;
};

std::ostream &operator<<(std::ostream &os, const UniformsParams &params)
{
    os << params.suffix().substr(1);
    return os;
}

std::string UniformsParams::suffix() const
{
    std::stringstream strstr;

    strstr << RenderTestParams::suffix();
    strstr << "_" << numVertexUniforms << "_vertex_uniforms";
    strstr << "_" << numFragmentUniforms << "_fragment_uniforms";

    return strstr.str();
}

class UniformsBenchmark : public ANGLERenderTest,
                          public ::testing::WithParamInterface<UniformsParams>
{
  public:
    UniformsBenchmark();

    void initializeBenchmark() override;
    void destroyBenchmark() override;
    void drawBenchmark() override;

  private:
    void initShaders();
    void initVertexBuffer();
    void initTextures();

    GLuint mProgram;
    std::vector<GLuint> mUniformLocations;
};

UniformsBenchmark::UniformsBenchmark() : ANGLERenderTest("Uniforms", GetParam()), mProgram(0u)
{
}

void UniformsBenchmark::initializeBenchmark()
{
    const auto &params = GetParam();

    ASSERT_GT(params.iterations, 0u);

    // Verify the uniform counts are within the limits
    GLint maxVertexUniforms, maxFragmentUniforms;
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxVertexUniforms);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &maxFragmentUniforms);

    if (params.numVertexUniforms > static_cast<size_t>(maxVertexUniforms))
    {
        FAIL() << "Vertex uniform count (" << params.numVertexUniforms << ")"
               << " exceeds maximum vertex uniform count: " << maxVertexUniforms << std::endl;
    }
    if (params.numFragmentUniforms > static_cast<size_t>(maxFragmentUniforms))
    {
        FAIL() << "Fragment uniform count (" << params.numFragmentUniforms << ")"
               << " exceeds maximum fragment uniform count: " << maxFragmentUniforms << std::endl;
    }

    initShaders();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glViewport(0, 0, getWindow()->getWidth(), getWindow()->getHeight());

    ASSERT_GL_NO_ERROR();
}

std::string GetUniformLocationName(size_t idx, bool vertexShader)
{
    std::stringstream strstr;
    strstr << (vertexShader ? "vs" : "fs") << "_u_" << idx;
    return strstr.str();
}

void UniformsBenchmark::initShaders()
{
    const auto &params = GetParam();

    std::stringstream vstrstr;
    vstrstr << "precision mediump float;\n";
    for (size_t i = 0; i < params.numVertexUniforms; i++)
    {
        vstrstr << "uniform vec4 " << GetUniformLocationName(i, true) << ";\n";
    }
    vstrstr << "void main()\n"
               "{\n"
               "    gl_Position = vec4(0, 0, 0, 0);\n";
    for (size_t i = 0; i < params.numVertexUniforms; i++)
    {
        vstrstr << "    gl_Position = gl_Position + " << GetUniformLocationName(i, true) << ";\n";
    }
    vstrstr << "}";

    std::stringstream fstrstr;
    fstrstr << "precision mediump float;\n";
    for (size_t i = 0; i < params.numFragmentUniforms; i++)
    {
        fstrstr << "uniform vec4 " << GetUniformLocationName(i, false) << ";\n";
    }
    fstrstr << "void main()\n"
               "{\n"
               "    gl_FragColor = vec4(0, 0, 0, 0);\n";
    for (size_t i = 0; i < params.numFragmentUniforms; i++)
    {
        fstrstr << "    gl_FragColor = gl_FragColor + " << GetUniformLocationName(i, false)
                << ";\n";
    }
    fstrstr << "}";

    mProgram = CompileProgram(vstrstr.str(), fstrstr.str());
    ASSERT_NE(0u, mProgram);

    for (size_t i = 0; i < params.numVertexUniforms; ++i)
    {
        GLint location = glGetUniformLocation(mProgram, GetUniformLocationName(i, true).c_str());
        ASSERT_NE(-1, location);
        mUniformLocations.push_back(location);
    }
    for (size_t i = 0; i < params.numFragmentUniforms; ++i)
    {
        GLint location = glGetUniformLocation(mProgram, GetUniformLocationName(i, false).c_str());
        ASSERT_NE(-1, location);
        mUniformLocations.push_back(location);
    }

    // Use the program object
    glUseProgram(mProgram);
}

void UniformsBenchmark::destroyBenchmark()
{
    glDeleteProgram(mProgram);
}

void UniformsBenchmark::drawBenchmark()
{
    const auto &params = GetParam();

    for (size_t it = 0; it < params.iterations; ++it)
    {
        for (size_t uniform = 0; uniform < mUniformLocations.size(); ++uniform)
        {
            float value = static_cast<float>(uniform);
            glUniform4f(mUniformLocations[uniform], value, value, value, value);
        }

        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    ASSERT_GL_NO_ERROR();
}

UniformsParams D3D11Params()
{
    UniformsParams params;
    params.eglParameters = egl_platform::D3D11();
    return params;
}

UniformsParams D3D9Params()
{
    UniformsParams params;
    params.eglParameters = egl_platform::D3D9();
    return params;
}

UniformsParams OpenGLParams()
{
    UniformsParams params;
    params.eglParameters = egl_platform::OPENGL();
    return params;
}

}  // anonymous namespace

TEST_P(UniformsBenchmark, Run)
{
    run();
}

ANGLE_INSTANTIATE_TEST(UniformsBenchmark, D3D11Params(), D3D9Params(), OpenGLParams());
