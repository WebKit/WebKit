//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ParallelLinkProgramPerfTest:
//   Tests performance of compiling and linking many shaders and programs in sequence.
//

#include "ANGLEPerfTest.h"

#include <array>

#include "common/vector_utils.h"
#include "util/shader_utils.h"

using namespace angle;

namespace
{

enum class CompileLinkOrder
{
    AllCompilesFirst,
    Interleaved,

    Unspecified,
};

struct ParallelLinkProgramParams final : public RenderTestParams
{
    ParallelLinkProgramParams(CompileLinkOrder order)
    {
        iterationsPerStep = 100;

        majorVersion     = 3;
        minorVersion     = 0;
        windowWidth      = 256;
        windowHeight     = 256;
        compileLinkOrder = order;
    }

    std::string story() const override
    {
        std::stringstream strstr;
        strstr << RenderTestParams::story();

        if (compileLinkOrder == CompileLinkOrder::AllCompilesFirst)
        {
            strstr << "_all_compiles_first";
        }
        else if (compileLinkOrder == CompileLinkOrder::Interleaved)
        {
            strstr << "_interleaved_compile_and_link";
        }

        if (eglParameters.deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE)
        {
            strstr << "_null";
        }

        return strstr.str();
    }

    CompileLinkOrder compileLinkOrder;
};

std::ostream &operator<<(std::ostream &os, const ParallelLinkProgramParams &params)
{
    os << params.backendAndStory().substr(1);
    return os;
}

class ParallelLinkProgramBenchmark : public ANGLERenderTest,
                                     public ::testing::WithParamInterface<ParallelLinkProgramParams>
{
  public:
    ParallelLinkProgramBenchmark();

    void initializeBenchmark() override;
    void destroyBenchmark() override;
    void drawBenchmark() override;

  protected:
    struct Program
    {
        std::string vertexShader;
        std::string fragmentShader;

        GLuint vs;
        GLuint fs;
        GLuint program;
    };

    std::vector<Program> mPrograms;
};

ParallelLinkProgramBenchmark::ParallelLinkProgramBenchmark()
    : ANGLERenderTest("ParallelLinkProgram", GetParam())
{
    if (IsWindows() && IsNVIDIA() &&
        GetParam().eglParameters.renderer == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
    {
        skipTest("http://angle.com/8410 crashes in the driver");
    }
}

void ParallelLinkProgramBenchmark::initializeBenchmark()
{
    const ParallelLinkProgramParams &params = GetParam();

    // Generate N shaders and create the GL objects.  DrawBenchmark would then _only_ do compilation
    // and link.
    mPrograms.resize(params.iterationsPerStep);
    for (uint32_t i = 0; i < params.iterationsPerStep; ++i)
    {
        std::ostringstream vs;
        vs << R"(#version 300 es
uniform UBO
{
    vec4 data[)"
           << params.iterationsPerStep << R"(];
};

uniform UBO2
{
    mat4 m[2];
} ubo;

in mediump vec4 attr1;
in mediump uvec4 attr2;

out highp vec3 var1;
flat out lowp uvec2 var2;

void main()
{
    vec4 a = attr1 + vec4(attr2);)";
        for (uint32_t j = 0; j < i * 5; ++j)
        {
            vs << R"(
    a *= ubo.m[0];)";
        }
        vs << R"(
    a += ubo.m[1][2];
    var1 = cross(a.zxy, data[)"
           << i << R"(].xxw);
    var2 = uvec2(1, 3) * uvec2(6, 3);
})";
        mPrograms[i].vertexShader = vs.str();

        std::ostringstream fs;
        fs << R"(#version 300 es
uniform UBO
{
    highp vec4 data[)"
           << params.iterationsPerStep << R"(];
};

in highp vec3 var1;
flat in lowp uvec2 var2;

uniform mediump sampler2D s;
uniform mediump sampler2D s2;

layout(location = 0) out mediump vec4 color1;
layout(location = 2) out highp uvec2 color2;

void main()
{
    color1 = var1.xyzx + texture(s, vec2(var2) / 9.);
    mediump vec4 sum = vec4(0);
    for (int i = 0; i < 10; ++i)
      sum += texture(s2, vec2(float(i / 2) / 5., float(i % 5) / 5.));
    uvec2 res = uvec2(sum.xz + sum.yw);)";
        for (uint32_t j = 0; j < i * 5; ++j)
        {
            fs << R"(
    res += uvec2(data[)"
               << (j % 100) << R"(]);)";
        }
        fs << R"(
    color2 = res;
})";
        mPrograms[i].fragmentShader = fs.str();

        mPrograms[i].vs      = glCreateShader(GL_VERTEX_SHADER);
        mPrograms[i].fs      = glCreateShader(GL_FRAGMENT_SHADER);
        mPrograms[i].program = glCreateProgram();

        const char *vsCStr = mPrograms[i].vertexShader.c_str();
        const char *fsCStr = mPrograms[i].fragmentShader.c_str();
        glShaderSource(mPrograms[i].vs, 1, &vsCStr, 0);
        glShaderSource(mPrograms[i].fs, 1, &fsCStr, 0);

        glAttachShader(mPrograms[i].program, mPrograms[i].vs);
        glAttachShader(mPrograms[i].program, mPrograms[i].fs);
    }

    ASSERT_GL_NO_ERROR();
}

void ParallelLinkProgramBenchmark::destroyBenchmark()
{
    const ParallelLinkProgramParams &params = GetParam();

    for (uint32_t i = 0; i < params.iterationsPerStep; ++i)
    {
        glDetachShader(mPrograms[i].program, mPrograms[i].vs);
        glDetachShader(mPrograms[i].program, mPrograms[i].fs);
        glDeleteShader(mPrograms[i].vs);
        glDeleteShader(mPrograms[i].fs);
        glDeleteProgram(mPrograms[i].program);
    }
}

void ParallelLinkProgramBenchmark::drawBenchmark()
{
    const ParallelLinkProgramParams &params = GetParam();

    for (uint32_t i = 0; i < params.iterationsPerStep; ++i)
    {
        // Compile the shaders, and if interleaved, link the corresponding programs.
        glCompileShader(mPrograms[i].vs);
        glCompileShader(mPrograms[i].fs);
        if (params.compileLinkOrder == CompileLinkOrder::Interleaved)
        {
            glLinkProgram(mPrograms[i].program);
        }
    }

    // If asked to link after all shaders are compiled, link all the programs now
    if (params.compileLinkOrder == CompileLinkOrder::AllCompilesFirst)
    {
        for (uint32_t i = 0; i < params.iterationsPerStep; ++i)
        {
            glLinkProgram(mPrograms[i].program);
        }
    }

    // Now that all the compile and link jobs have been scheduled, wait for them all to finish.
    for (uint32_t i = 0; i < params.iterationsPerStep; ++i)
    {
        GLint compileResult;
        glGetShaderiv(mPrograms[i].vs, GL_COMPILE_STATUS, &compileResult);
        EXPECT_NE(compileResult, 0);
        glGetShaderiv(mPrograms[i].fs, GL_COMPILE_STATUS, &compileResult);
        EXPECT_NE(compileResult, 0);

        GLint linkStatus = GL_TRUE;
        glGetProgramiv(mPrograms[i].program, GL_LINK_STATUS, &linkStatus);
        EXPECT_TRUE(linkStatus);
    }

    ASSERT_GL_NO_ERROR();
}

using namespace egl_platform;

ParallelLinkProgramParams ParallelLinkProgramD3D11Params(CompileLinkOrder compileLinkOrder)
{
    ParallelLinkProgramParams params(compileLinkOrder);
    params.eglParameters = D3D11();
    return params;
}

ParallelLinkProgramParams ParallelLinkProgramMetalParams(CompileLinkOrder compileLinkOrder)
{
    ParallelLinkProgramParams params(compileLinkOrder);
    params.eglParameters = METAL();
    return params;
}

ParallelLinkProgramParams ParallelLinkProgramOpenGLOrGLESParams(CompileLinkOrder compileLinkOrder)
{
    ParallelLinkProgramParams params(compileLinkOrder);
    params.eglParameters = OPENGL_OR_GLES();
    return params;
}

ParallelLinkProgramParams ParallelLinkProgramVulkanParams(CompileLinkOrder compileLinkOrder)
{
    ParallelLinkProgramParams params(compileLinkOrder);
    params.eglParameters = VULKAN();
    params.enable(Feature::EnableParallelCompileAndLink);
    return params;
}

// Test parallel link performance
TEST_P(ParallelLinkProgramBenchmark, Run)
{
    run();
}

ANGLE_INSTANTIATE_TEST(ParallelLinkProgramBenchmark,
                       ParallelLinkProgramD3D11Params(CompileLinkOrder::AllCompilesFirst),
                       ParallelLinkProgramD3D11Params(CompileLinkOrder::Interleaved),
                       ParallelLinkProgramMetalParams(CompileLinkOrder::AllCompilesFirst),
                       ParallelLinkProgramMetalParams(CompileLinkOrder::Interleaved),
                       ParallelLinkProgramOpenGLOrGLESParams(CompileLinkOrder::AllCompilesFirst),
                       ParallelLinkProgramOpenGLOrGLESParams(CompileLinkOrder::Interleaved),
                       ParallelLinkProgramVulkanParams(CompileLinkOrder::AllCompilesFirst),
                       ParallelLinkProgramVulkanParams(CompileLinkOrder::Interleaved));

}  // anonymous namespace
