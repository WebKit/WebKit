//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SamplerMultisample_test.cpp:
// Tests compiling shaders that use gsampler2DMS types
//

#include "angle_gl.h"
#include "gtest/gtest.h"
#include "GLSLANG/ShaderLang.h"
#include "tests/test_utils/ShaderCompileTreeTest.h"

using namespace sh;

class SamplerMultisampleTest : public ShaderCompileTreeTest
{
  public:
    SamplerMultisampleTest() {}

  protected:
    ::GLenum getShaderType() const override { return GL_FRAGMENT_SHADER; }
    ShShaderSpec getShaderSpec() const override { return SH_GLES3_1_SPEC; }
};

// checks whether compiler has parsed the gsampler2DMS, texelfetch qualifiers correctly
TEST_F(SamplerMultisampleTest, TexelFetchSampler2DMSQualifier)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision highp float;\n"
        "uniform highp sampler2DMS s;\n"
        "uniform highp isampler2DMS is;\n"
        "uniform highp usampler2DMS us;\n"
        ""
        "void main() {\n"
        "    vec4 tex1 = texelFetch(s, ivec2(0, 0), 0);\n"
        "    ivec4 tex2 = texelFetch(is, ivec2(0, 0), 0);\n"
        "    uvec4 tex3 = texelFetch(us, ivec2(0, 0), 0);\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// checks whether compiler has parsed the gsampler2DMS, texturesize qualifiers correctly
TEST_F(SamplerMultisampleTest, TextureSizeSampler2DMSQualifier)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision highp float;\n"
        "uniform highp sampler2DMS s;\n"
        "uniform highp isampler2DMS is;\n"
        "uniform highp usampler2DMS us;\n"
        ""
        "void main() {\n"
        "    ivec2 size = textureSize(s);\n"
        "    size = textureSize(is);\n"
        "    size = textureSize(us);\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// checks gsampler2DMS has no default precision
TEST_F(SamplerMultisampleTest, NoPrecisionSampler2DMS)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision highp float;\n"
        "uniform sampler2DMS s;\n"
        "uniform isampler2DMS is;\n"
        "uniform usampler2DMS us;\n"
        ""
        "void main() {}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}
