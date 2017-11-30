//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// LinkProgramPerfTest:
//   Performance tests compiling a lot of shaders.
//

#include "ANGLEPerfTest.h"

#include <array>

#include "common/vector_utils.h"
#include "shader_utils.h"

using namespace angle;

namespace
{

struct LinkProgramParams final : public RenderTestParams
{
    LinkProgramParams()
    {
        majorVersion = 2;
        minorVersion = 0;
        windowWidth  = 256;
        windowHeight = 256;
    }

    std::string suffix() const override
    {
        std::stringstream strstr;
        strstr << RenderTestParams::suffix();

        if (eglParameters.deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE)
        {
            strstr << "_null";
        }

        return strstr.str();
    }
};

std::ostream &operator<<(std::ostream &os, const LinkProgramParams &params)
{
    os << params.suffix().substr(1);
    return os;
}

class LinkProgramBenchmark : public ANGLERenderTest,
                             public ::testing::WithParamInterface<LinkProgramParams>
{
  public:
    LinkProgramBenchmark();

    void initializeBenchmark() override;
    void destroyBenchmark() override;
    void drawBenchmark() override;

  protected:
    GLuint mVertexBuffer = 0;
};

LinkProgramBenchmark::LinkProgramBenchmark() : ANGLERenderTest("LinkProgram", GetParam())
{
}

void LinkProgramBenchmark::initializeBenchmark()
{
    std::array<Vector3, 6> vertices = {{Vector3(-1.0f, 1.0f, 0.5f), Vector3(-1.0f, -1.0f, 0.5f),
                                        Vector3(1.0f, -1.0f, 0.5f), Vector3(-1.0f, 1.0f, 0.5f),
                                        Vector3(1.0f, -1.0f, 0.5f), Vector3(1.0f, 1.0f, 0.5f)}};

    glGenBuffers(1, &mVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vector3), vertices.data(),
                 GL_STATIC_DRAW);
};

void LinkProgramBenchmark::destroyBenchmark()
{
    glDeleteBuffers(1, &mVertexBuffer);
}

void LinkProgramBenchmark::drawBenchmark()
{
    static const char *vertexShader =
        "attribute vec2 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 0, 1);\n"
        "}";
    static const char *fragmentShader =
        "precision mediump float;\n"
        "void main() {\n"
        "    gl_FragColor = vec4(1, 0, 0, 1);\n"
        "}";

    GLuint program = CompileProgram(vertexShader, fragmentShader);
    ASSERT_NE(0u, program);

    glUseProgram(program);

    GLint positionLoc = glGetAttribLocation(program, "position");
    glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
    glEnableVertexAttribArray(positionLoc);

    // Draw with the program to ensure the shader gets compiled and used.
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDeleteProgram(program);
}

using namespace egl_platform;

LinkProgramParams LinkProgramD3D11Params()
{
    LinkProgramParams params;
    params.eglParameters = D3D11();
    return params;
}

LinkProgramParams LinkProgramD3D9Params()
{
    LinkProgramParams params;
    params.eglParameters = D3D9();
    return params;
}

LinkProgramParams LinkProgramOpenGLOrGLESParams()
{
    LinkProgramParams params;
    params.eglParameters = OPENGL_OR_GLES(false);
    return params;
}

TEST_P(LinkProgramBenchmark, Run)
{
    run();
}

ANGLE_INSTANTIATE_TEST(LinkProgramBenchmark,
                       LinkProgramD3D11Params(),
                       LinkProgramD3D9Params(),
                       LinkProgramOpenGLOrGLESParams());

}  // anonymous namespace
