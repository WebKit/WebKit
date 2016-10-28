//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ComputeShaderTest:
//   Compute shader specific tests.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include <vector>

using namespace angle;

namespace
{

class ComputeShaderTest : public ANGLETest
{
  protected:
    ComputeShaderTest() {}
};

class ComputeShaderTestES3 : public ANGLETest
{
  protected:
    ComputeShaderTestES3() {}
};

// link a simple compute program. It should be successful.
TEST_P(ComputeShaderTest, LinkComputeProgram)
{
    if (IsIntel() && IsLinux())
    {
        std::cout << "Test skipped on Intel Linux due to failures." << std::endl;
        return;
    }

    const std::string csSource =
        "#version 310 es\n"
        "layout(local_size_x=1) in;\n"
        "void main()\n"
        "{\n"
        "}\n";

    ANGLE_GL_COMPUTE_PROGRAM(program, csSource);

    EXPECT_GL_NO_ERROR();
}

// link a simple compute program. There is no local size and linking should fail.
TEST_P(ComputeShaderTest, LinkComputeProgramNoLocalSizeLinkError)
{
    const std::string csSource =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "}\n";

    GLuint program = CompileComputeProgram(csSource, false);
    EXPECT_EQ(0u, program);

    glDeleteProgram(program);

    EXPECT_GL_NO_ERROR();
}

// link a simple compute program.
// make sure that uniforms and uniform samplers get recorded
TEST_P(ComputeShaderTest, LinkComputeProgramWithUniforms)
{
    if (IsIntel() && IsLinux())
    {
        std::cout << "Test skipped on Intel Linux due to failures." << std::endl;
        return;
    }
    const std::string csSource =
        "#version 310 es\n"
        "precision mediump sampler2D;\n"
        "layout(local_size_x=1) in;\n"
        "uniform int myUniformInt;\n"
        "uniform sampler2D myUniformSampler;\n"
        "void main()\n"
        "{\n"
        "int q = myUniformInt;\n"
        "texture(myUniformSampler, vec2(0.0));\n"
        "}\n";

    ANGLE_GL_COMPUTE_PROGRAM(program, csSource);

    GLint uniformLoc = glGetUniformLocation(program.get(), "myUniformInt");
    EXPECT_NE(-1, uniformLoc);

    uniformLoc = glGetUniformLocation(program.get(), "myUniformSampler");
    EXPECT_NE(-1, uniformLoc);

    EXPECT_GL_NO_ERROR();
}

// Attach both compute and non-compute shaders. A link time error should occur.
// OpenGL ES 3.10, 7.3 Program Objects
TEST_P(ComputeShaderTest, AttachMultipleShaders)
{
    if (IsIntel() && IsLinux())
    {
        std::cout << "Test skipped on Intel Linux due to failures." << std::endl;
        return;
    }
    const std::string csSource =
        "#version 310 es\n"
        "layout(local_size_x=1) in;\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string vsSource =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string fsSource =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "}\n";

    GLuint program = glCreateProgram();

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);
    GLuint cs = CompileShader(GL_COMPUTE_SHADER, csSource);

    EXPECT_NE(0u, vs);
    EXPECT_NE(0u, fs);
    EXPECT_NE(0u, cs);

    glAttachShader(program, vs);
    glDeleteShader(vs);

    glAttachShader(program, fs);
    glDeleteShader(fs);

    glAttachShader(program, cs);
    glDeleteShader(cs);

    glLinkProgram(program);

    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);

    EXPECT_EQ(0, linkStatus);

    EXPECT_GL_NO_ERROR();
}

// Attach a vertex, fragment and compute shader.
// Query for the number of attached shaders and check the count.
TEST_P(ComputeShaderTest, AttachmentCount)
{
    if (IsIntel() && IsLinux())
    {
        std::cout << "Test skipped on Intel Linux due to failures." << std::endl;
        return;
    }
    const std::string csSource =
        "#version 310 es\n"
        "layout(local_size_x=1) in;\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string vsSource =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string fsSource =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "}\n";

    GLuint program = glCreateProgram();

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);
    GLuint cs = CompileShader(GL_COMPUTE_SHADER, csSource);

    EXPECT_NE(0u, vs);
    EXPECT_NE(0u, fs);
    EXPECT_NE(0u, cs);

    glAttachShader(program, vs);
    glDeleteShader(vs);

    glAttachShader(program, fs);
    glDeleteShader(fs);

    glAttachShader(program, cs);
    glDeleteShader(cs);

    GLint numAttachedShaders;
    glGetProgramiv(program, GL_ATTACHED_SHADERS, &numAttachedShaders);

    EXPECT_EQ(3, numAttachedShaders);

    glDeleteProgram(program);

    EXPECT_GL_NO_ERROR();
}

// Check that it is not possible to create a compute shader when the context does not support ES
// 3.10
TEST_P(ComputeShaderTestES3, NotSupported)
{
    GLuint computeShaderHandle = glCreateShader(GL_COMPUTE_SHADER);
    EXPECT_EQ(0u, computeShaderHandle);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

ANGLE_INSTANTIATE_TEST(ComputeShaderTest, ES31_OPENGL(), ES31_OPENGLES());
ANGLE_INSTANTIATE_TEST(ComputeShaderTestES3, ES3_OPENGL(), ES3_OPENGLES());

}  // namespace
