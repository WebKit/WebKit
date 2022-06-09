//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EXT_YUV_target.cpp:
//   Test for EXT_YUV_target implementation.
//

#include "tests/test_utils/ShaderExtensionTest.h"

using EXTYUVTargetTest = sh::ShaderExtensionTest;

namespace
{
const char EXTYTPragma[] = "#extension GL_EXT_YUV_target : require\n";

const char ESSL300_SimpleShader[] =
    "precision mediump float;\n"
    "uniform __samplerExternal2DY2YEXT uSampler;\n"
    "out vec4 fragColor;\n"
    "void main() { \n"
    "    fragColor = vec4(1.0);\n"
    "}\n";

// Shader that samples the texture and writes to FragColor.
const char ESSL300_FragColorShader[] =
    "precision mediump float;\n"
    "uniform __samplerExternal2DY2YEXT uSampler;\n"
    "layout(yuv) out vec4 fragColor;\n"
    "void main() { \n"
    "    fragColor = texture(uSampler, vec2(0.0));\n"
    "}\n";

// Shader that specifies yuv layout qualifier multiple times.
const char ESSL300_YUVQualifierMultipleTimesShader[] =
    "precision mediump float;\n"
    "layout(yuv, yuv, yuv) out vec4 fragColor;\n"
    "void main() { \n"
    "}\n";

// Shader that specifies yuv layout qualifier for not output fails to compile.
const char ESSL300_YUVQualifierFailureShader1[] =
    "precision mediump float;\n"
    "layout(yuv) in vec4 fragColor;\n"
    "void main() { \n"
    "}\n";

const char ESSL300_YUVQualifierFailureShader2[] =
    "precision mediump float;\n"
    "layout(yuv) uniform;\n"
    "layout(yuv) uniform Transform {\n"
    "     mat4 M1;\n"
    "}\n"
    "void main() { \n"
    "}\n";

// Shader that specifies yuv layout qualifier with location fails to compile.
const char ESSL300_LocationAndYUVFailureShader[] =
    "precision mediump float;\n"
    "layout(location = 0, yuv) out vec4 fragColor;\n"
    "void main() { \n"
    "}\n";

// Shader that specifies yuv layout qualifier with multiple color outputs fails to compile.
const char ESSL300_MultipleColorAndYUVOutputsFailureShader1[] =
    "precision mediump float;\n"
    "layout(yuv) out vec4 fragColor;\n"
    "layout out vec4 fragColor1;\n"
    "void main() { \n"
    "}\n";

const char ESSL300_MultipleColorAndYUVOutputsFailureShader2[] =
    "precision mediump float;\n"
    "layout(yuv) out vec4 fragColor;\n"
    "layout(location = 1) out vec4 fragColor1;\n"
    "void main() { \n"
    "}\n";

// Shader that specifies yuv layout qualifier with depth output fails to compile.
const char ESSL300_DepthAndYUVOutputsFailureShader[] =
    "precision mediump float;\n"
    "layout(yuv) out vec4 fragColor;\n"
    "void main() { \n"
    "    gl_FragDepth = 1.0f;\n"
    "}\n";

// Shader that specifies yuv layout qualifier with multiple outputs fails to compile.
const char ESSL300_MultipleYUVOutputsFailureShader[] =
    "precision mediump float;\n"
    "layout(yuv) out vec4 fragColor;\n"
    "layout(yuv) out vec4 fragColor1;\n"
    "void main() { \n"
    "}\n";

// Shader that specifies yuvCscStandartEXT type and associated values.
const char ESSL300_YuvCscStandardEXTShader[] =
    R"(precision mediump float;
    yuvCscStandardEXT;
    yuvCscStandardEXT conv;
    yuvCscStandardEXT conv1 = itu_601;
    yuvCscStandardEXT conv2 = itu_601_full_range;
    yuvCscStandardEXT conv3 = itu_709;
    const yuvCscStandardEXT conv4 = itu_709;

    uniform int u;
    out vec4 my_color;

    yuvCscStandardEXT conv_standard()
    {
        switch(u)
        {
            case 1:
                return conv1;
            case 2:
                return conv2;
            case 3:
                return conv3;
            default:
                return conv;
        }
    }
    bool is_itu_601(inout yuvCscStandardEXT csc)
    {
        csc = itu_601;
        return csc == itu_601;
    }
    bool is_itu_709(yuvCscStandardEXT csc)
    {
        return csc == itu_709;
    }
    void main()
    {
        yuvCscStandardEXT conv = conv_standard();
        bool csc_check1 = is_itu_601(conv);
        bool csc_check2 = is_itu_709(itu_709);
        if (csc_check1 && csc_check2) {
            my_color = vec4(0, 1, 0, 1);
        }
    })";

// Shader that specifies yuvCscStandartEXT type constructor fails to compile.
const char ESSL300_YuvCscStandartdEXTConstructFailureShader1[] =
    "precision mediump float;\n"
    "yuvCscStandardEXT conv = yuvCscStandardEXT();\n"
    "void main() { \n"
    "}\n";

const char ESSL300_YuvCscStandartdEXTConstructFailureShader2[] =
    "precision mediump float;\n"
    "yuvCscStandardEXT conv = yuvCscStandardEXT(itu_601);\n"
    "void main() { \n"
    "}\n";

// Shader that specifies yuvCscStandartEXT type conversion fails to compile.
const char ESSL300_YuvCscStandartdEXTConversionFailureShader1[] =
    "precision mediump float;\n"
    "yuvCscStandardEXT conv = false;\n"
    "void main() { \n"
    "}\n";

const char ESSL300_YuvCscStandartdEXTConversionFailureShader2[] =
    "precision mediump float;\n"
    "yuvCscStandardEXT conv = 0;\n"
    "void main() { \n"
    "}\n";

const char ESSL300_YuvCscStandartdEXTConversionFailureShader3[] =
    "precision mediump float;\n"
    "yuvCscStandardEXT conv = 2.0f;\n"
    "void main() { \n"
    "}\n";

const char ESSL300_YuvCscStandartdEXTConversionFailureShader4[] =
    "precision mediump float;\n"
    "yuvCscStandardEXT conv = itu_601 | itu_709;\n"
    "void main() { \n"
    "}\n";

const char ESSL300_YuvCscStandartdEXTConversionFailureShader5[] =
    "precision mediump float;\n"
    "yuvCscStandardEXT conv = itu_601 & 3.0f;\n"
    "void main() { \n"
    "}\n";

// Shader that specifies yuvCscStandartEXT type qualifiers fails to compile.
const char ESSL300_YuvCscStandartdEXTQualifiersFailureShader1[] =
    "precision mediump float;\n"
    "in yuvCscStandardEXT conv = itu_601;\n"
    "void main() { \n"
    "}\n";

const char ESSL300_YuvCscStandartdEXTQualifiersFailureShader2[] =
    "precision mediump float;\n"
    "out yuvCscStandardEXT conv = itu_601;\n"
    "void main() { \n"
    "}\n";

const char ESSL300_YuvCscStandartdEXTQualifiersFailureShader3[] =
    "precision mediump float;\n"
    "uniform yuvCscStandardEXT conv = itu_601;\n"
    "void main() { \n"
    "}\n";

// Shader that specifies yuv_to_rgb() and rgb_to_yuv() built-in functions.
const char ESSL300_BuiltInFunctionsShader[] =
    R"(precision mediump float;
    yuvCscStandardEXT conv = itu_601;

    out vec4 my_color;

    void main()
    {
        vec3 yuv = rgb_2_yuv(vec3(0.0f), conv);
        vec3 rgb = yuv_2_rgb(yuv, itu_601);
        my_color = vec4(rgb, 1.0);
    })";

const char ESSL300_OverloadRgb2Yuv[] =
    R"(precision mediump float;
    float rgb_2_yuv(float x) { return x + 1.0; }

    in float i;
    out float o;

    void main()
    {
        o = rgb_2_yuv(i);
    })";

const char ESSL300_OverloadYuv2Rgb[] =
    R"(precision mediump float;
    float yuv_2_rgb(float x) { return x + 1.0; }

    in float i;
    out float o;

    void main()
    {
        o = yuv_2_rgb(i);
    })";

// Extension flag is required to compile properly. Expect failure when it is
// not present.
TEST_P(EXTYUVTargetTest, CompileFailsWithoutExtension)
{
    mResources.EXT_YUV_target = 0;
    InitializeCompiler();
    EXPECT_FALSE(TestShaderCompile(EXTYTPragma));
}

// Extension directive is required to compile properly. Expect failure when
// it is not present.
TEST_P(EXTYUVTargetTest, CompileFailsWithExtensionWithoutPragma)
{
    mResources.EXT_YUV_target = 1;
    InitializeCompiler();
    EXPECT_FALSE(TestShaderCompile(""));
}

// With extension flag and extension directive, compiling succeeds.
// Also test that the extension directive state is reset correctly.
TEST_P(EXTYUVTargetTest, CompileSucceedsWithExtensionAndPragma)
{
    mResources.EXT_YUV_target = 1;
    InitializeCompiler();
    EXPECT_TRUE(TestShaderCompile(EXTYTPragma));
    // Test reset functionality.
    EXPECT_FALSE(TestShaderCompile(""));
    EXPECT_TRUE(TestShaderCompile(EXTYTPragma));
}

INSTANTIATE_TEST_SUITE_P(CorrectVariantsWithExtensionAndPragma,
                         EXTYUVTargetTest,
                         Combine(Values(SH_GLES3_SPEC),
                                 Values(sh::ESSLVersion300),
                                 Values(ESSL300_SimpleShader, ESSL300_FragColorShader)));

class EXTYUVTargetCompileSuccessTest : public EXTYUVTargetTest
{};

TEST_P(EXTYUVTargetCompileSuccessTest, CompileSucceeds)
{
    // Expect compile success.
    mResources.EXT_YUV_target = 1;
    InitializeCompiler();
    EXPECT_TRUE(TestShaderCompile(EXTYTPragma));
}

INSTANTIATE_TEST_SUITE_P(CorrectESSL300Shaders,
                         EXTYUVTargetCompileSuccessTest,
                         Combine(Values(SH_GLES3_SPEC),
                                 Values(sh::ESSLVersion300),
                                 Values(ESSL300_FragColorShader,
                                        ESSL300_YUVQualifierMultipleTimesShader,
                                        ESSL300_YuvCscStandardEXTShader,
                                        ESSL300_BuiltInFunctionsShader)));

class EXTYUVTargetCompileFailureTest : public EXTYUVTargetTest
{};

TEST_P(EXTYUVTargetCompileFailureTest, CompileFails)
{
    // Expect compile failure due to shader error, with shader having correct pragma.
    mResources.EXT_YUV_target = 1;
    InitializeCompiler();
    EXPECT_FALSE(TestShaderCompile(EXTYTPragma));
}

INSTANTIATE_TEST_SUITE_P(IncorrectESSL300Shaders,
                         EXTYUVTargetCompileFailureTest,
                         Combine(Values(SH_GLES3_SPEC),
                                 Values(sh::ESSLVersion300),
                                 Values(ESSL300_YUVQualifierFailureShader1,
                                        ESSL300_YUVQualifierFailureShader2,
                                        ESSL300_LocationAndYUVFailureShader,
                                        ESSL300_MultipleColorAndYUVOutputsFailureShader1,
                                        ESSL300_MultipleColorAndYUVOutputsFailureShader2,
                                        ESSL300_DepthAndYUVOutputsFailureShader,
                                        ESSL300_MultipleYUVOutputsFailureShader,
                                        ESSL300_YuvCscStandartdEXTConstructFailureShader1,
                                        ESSL300_YuvCscStandartdEXTConstructFailureShader2,
                                        ESSL300_YuvCscStandartdEXTConversionFailureShader1,
                                        ESSL300_YuvCscStandartdEXTConversionFailureShader2,
                                        ESSL300_YuvCscStandartdEXTConversionFailureShader3,
                                        ESSL300_YuvCscStandartdEXTConversionFailureShader4,
                                        ESSL300_YuvCscStandartdEXTConversionFailureShader5,
                                        ESSL300_YuvCscStandartdEXTQualifiersFailureShader1,
                                        ESSL300_YuvCscStandartdEXTQualifiersFailureShader2,
                                        ESSL300_YuvCscStandartdEXTQualifiersFailureShader3)));

class EXTYUVNotEnabledTest : public EXTYUVTargetTest
{};

TEST_P(EXTYUVNotEnabledTest, CanOverloadConversions)
{
    // Expect compile success with a shader that overloads functions in the EXT_YUV_target
    // extension.
    mResources.EXT_YUV_target = 0;
    InitializeCompiler();
    EXPECT_TRUE(TestShaderCompile(""));
}

INSTANTIATE_TEST_SUITE_P(CoreESSL300Shaders,
                         EXTYUVNotEnabledTest,
                         Combine(Values(SH_GLES3_SPEC),
                                 Values(sh::ESSLVersion300),
                                 Values(ESSL300_OverloadRgb2Yuv, ESSL300_OverloadYuv2Rgb)));

}  // namespace
