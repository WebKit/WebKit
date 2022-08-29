//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EXT_clip_cull_distance_test.cpp:
//   Test for EXT_clip_cull_distance
//

#include "tests/test_utils/ShaderExtensionTest.h"

namespace
{
const char EXTPragma[] = "#extension GL_EXT_clip_cull_distance : require\n";

// Shader using gl_ClipDistance and gl_CullDistance
const char VertexShaderCompileSucceeds1[] =
    R"(
    uniform vec4 uPlane;

    in vec4 aPosition;

    void main()
    {
        gl_Position = aPosition;
        gl_ClipDistance[1] = dot(aPosition, uPlane);
        gl_CullDistance[1] = dot(aPosition, uPlane);
    })";

// Shader redeclares gl_ClipDistance and gl_CullDistance
const char VertexShaderCompileSucceeds2[] =
    R"(
    uniform vec4 uPlane;

    in vec4 aPosition;

    out highp float gl_ClipDistance[4];
    out highp float gl_CullDistance[4];

    void main()
    {
        gl_Position = aPosition;
        gl_ClipDistance[gl_MaxClipDistances - 6 + 1] = dot(aPosition, uPlane);
        gl_ClipDistance[gl_MaxClipDistances - int(aPosition.x)] = dot(aPosition, uPlane);
        gl_CullDistance[gl_MaxCullDistances - 6 + 1] = dot(aPosition, uPlane);
        gl_CullDistance[gl_MaxCullDistances - int(aPosition.x)] = dot(aPosition, uPlane);
    })";

#if defined(ANGLE_ENABLE_VULKAN)
// Shader using gl_ClipDistance and gl_CullDistance
// But, the sum of the sizes is greater than gl_MaxCombinedClipAndCullDistances
const char VertexShaderCompileFails1[] =
    R"(
    uniform vec4 uPlane;

    in vec4 aPosition;

    void main()
    {
        gl_Position = aPosition;
        gl_ClipDistance[5] = dot(aPosition, uPlane);
        gl_CullDistance[4] = dot(aPosition, uPlane);
    })";

// Shader redeclares gl_ClipDistance and gl_CullDistance
// But, the sum of the sizes is greater than gl_MaxCombinedClipAndCullDistances
const char VertexShaderCompileFails2[] =
    R"(
    uniform vec4 uPlane;

    in vec4 aPosition;

    out highp float gl_ClipDistance[5];
    out highp float gl_CullDistance[4];

    void main()
    {
        gl_Position = aPosition;
        gl_ClipDistance[gl_MaxClipDistances - 6 + 1] = dot(aPosition, uPlane);
        gl_ClipDistance[gl_MaxClipDistances - int(aPosition.x)] = dot(aPosition, uPlane);
        gl_CullDistance[gl_MaxCullDistances - 6 + 1] = dot(aPosition, uPlane);
        gl_CullDistance[gl_MaxCullDistances - int(aPosition.x)] = dot(aPosition, uPlane);
    })";

// Shader redeclares gl_ClipDistance
// But, the array size is greater than gl_MaxClipDistances
const char VertexShaderCompileFails3[] =
    R"(
    uniform vec4 uPlane;

    in vec4 aPosition;

    out highp float gl_ClipDistance[gl_MaxClipDistances + 1];

    void main()
    {
        gl_Position = aPosition;
        gl_ClipDistance[gl_MaxClipDistances - 6 + 1] = dot(aPosition, uPlane);
        gl_ClipDistance[gl_MaxClipDistances - int(aPosition.x)] = dot(aPosition, uPlane);
    })";

// Access gl_CullDistance with integral constant index
// But, the index is greater than gl_MaxCullDistances
const char VertexShaderCompileFails4[] =
    R"(
    uniform vec4 uPlane;

    in vec4 aPosition;

    void main()
    {
        gl_Position = aPosition;
        gl_CullDistance[gl_MaxCullDistances] = dot(aPosition, uPlane);
    })";

const char VertexShaderCompileFails5[] =
    R"(
    uniform vec4 uPlane;

    attribute vec4 aPosition;

    void main()
    {
        gl_Position = aPosition;
        gl_CullDistance[0] = 0.0;
    })";

const char VertexShaderCompileFailes6[] =
    R"(
    uniform vec4 uPlane;

    attribute vec4 aPosition;

    varying float gl_ClipDistance[1];

    void main()
    {
        gl_Position = aPosition;
        gl_ClipDistance[0] = 0.0;
    })";
#endif

// Shader using gl_ClipDistance and gl_CullDistance
const char FragmentShaderCompileSucceeds1[] =
    R"(
    out highp vec4 fragColor;

    void main()
    {
        fragColor = vec4(gl_ClipDistance[0], gl_CullDistance[0], 0, 1);
    })";

// Shader redeclares gl_ClipDistance and gl_CullDistance
const char FragmentShaderCompileSucceeds2[] =
    R"(
    in highp float gl_ClipDistance[4];
    in highp float gl_CullDistance[4];

    in highp vec4 aPosition;

    out highp vec4 fragColor;

    void main()
    {
        fragColor.x = gl_ClipDistance[gl_MaxClipDistances - 6 + 1];
        fragColor.y = gl_ClipDistance[gl_MaxClipDistances - int(aPosition.x)];
        fragColor.z = gl_CullDistance[gl_MaxCullDistances - 6 + 1];
        fragColor.w = gl_CullDistance[gl_MaxCullDistances - int(aPosition.x)];
    })";

#if defined(ANGLE_ENABLE_VULKAN)
// Shader using gl_ClipDistance and gl_CullDistance
// But, the sum of the sizes is greater than gl_MaxCombinedClipAndCullDistances
const char FragmentShaderCompileFails1[] =
    R"(
    out highp vec4 fragColor;

    void main()
    {
        fragColor = vec4(gl_ClipDistance[4], gl_CullDistance[5], 0, 1);
    })";

// Shader redeclares gl_ClipDistance and gl_CullDistance
// But, the sum of the sizes is greater than gl_MaxCombinedClipAndCullDistances
const char FragmentShaderCompileFails2[] =
    R"(
    in highp float gl_ClipDistance[5];
    in highp float gl_CullDistance[4];

    in highp vec4 aPosition;

    out highp vec4 fragColor;

    void main()
    {
        fragColor.x = gl_ClipDistance[gl_MaxClipDistances - 6 + 1];
        fragColor.y = gl_ClipDistance[gl_MaxClipDistances - int(aPosition.x)];
        fragColor.z = gl_CullDistance[gl_MaxCullDistances - 6 + 1];
        fragColor.w = gl_CullDistance[gl_MaxCullDistances - int(aPosition.x)];
    })";

// In fragment shader, writing to gl_ClipDistance should be denied.
const char FragmentShaderCompileFails3[] =
    R"(
    out highp vec4 fragColor;

    void main()
    {
        gl_ClipDistance[0] = 0.0f;
        fragColor = vec4(1, gl_ClipDistance[0], 0, 1);
    })";

// In fragment shader, writing to gl_CullDistance should be denied even if redeclaring it with the
// array size
const char FragmentShaderCompileFails4[] =
    R"(
    out highp vec4 fragColor;

    in highp float gl_CullDistance[1];

    void main()
    {
        gl_CullDistance[0] = 0.0f;
        fragColor = vec4(1, gl_CullDistance[0], 0, 1);
    })";

// Accessing to gl_Clip/CullDistance with non-const index should be denied if the size of
// gl_Clip/CullDistance is not decided.
const char FragmentShaderCompileFails5[] =
    R"(
    out highp vec4 fragColor;

    void main()
    {
        medium float color[3];
        for(int i = 0 ; i < 3 ; i++)
        {
            color[i] = gl_CullDistance[i];
        }
        fragColor = vec4(color[0], color[1], color[2], 1.0f);
    })";
#endif

class EXTClipCullDistanceTest : public sh::ShaderExtensionTest
{
  public:
    void InitializeCompiler(ShShaderOutput shaderOutputType, GLenum shaderType)
    {
        DestroyCompiler();

        mCompiler = sh::ConstructCompiler(shaderType, testing::get<0>(GetParam()), shaderOutputType,
                                          &mResources);
        ASSERT_TRUE(mCompiler != nullptr) << "Compiler could not be constructed.";
    }

    testing::AssertionResult TestShaderCompile(const char *pragma)
    {
        ShCompileOptions compileOptions = {};
        compileOptions.objectCode       = true;
        compileOptions.variables        = true;

        const char *shaderStrings[] = {testing::get<1>(GetParam()), pragma,
                                       testing::get<2>(GetParam())};
        bool success                = sh::Compile(mCompiler, shaderStrings, 3, compileOptions);
        if (success)
        {
            return ::testing::AssertionSuccess() << "Compilation success";
        }
        return ::testing::AssertionFailure() << sh::GetInfoLog(mCompiler);
    }

    void SetExtensionEnable(bool enable)
    {
        // GL_APPLE_clip_distance is implicitly enabled when GL_EXT_clip_cull_distance is enabled
#if defined(ANGLE_ENABLE_VULKAN)
        mResources.APPLE_clip_distance = enable;
#endif
        mResources.EXT_clip_cull_distance = enable;
    }
};

class EXTClipCullDistanceForVertexShaderTest : public EXTClipCullDistanceTest
{
  public:
    void InitializeCompiler() { InitializeCompiler(SH_GLSL_450_CORE_OUTPUT); }
    void InitializeCompiler(ShShaderOutput shaderOutputType)
    {
        EXTClipCullDistanceTest::InitializeCompiler(shaderOutputType, GL_VERTEX_SHADER);
    }
};

class EXTClipCullDistanceForFragmentShaderTest : public EXTClipCullDistanceTest
{
  public:
    void InitializeCompiler() { InitializeCompiler(SH_GLSL_450_CORE_OUTPUT); }
    void InitializeCompiler(ShShaderOutput shaderOutputType)
    {
        EXTClipCullDistanceTest::InitializeCompiler(shaderOutputType, GL_FRAGMENT_SHADER);
    }
};

// Extension flag is required to compile properly. Expect failure when it is
// not present.
TEST_P(EXTClipCullDistanceForVertexShaderTest, CompileFailsWithoutExtension)
{
    SetExtensionEnable(false);
    InitializeCompiler();
    EXPECT_FALSE(TestShaderCompile(EXTPragma));
}

// Extension directive is required to compile properly. Expect failure when
// it is not present.
TEST_P(EXTClipCullDistanceForVertexShaderTest, CompileFailsWithExtensionWithoutPragma)
{
    SetExtensionEnable(true);
    InitializeCompiler();
    EXPECT_FALSE(TestShaderCompile(""));
}

#if defined(ANGLE_ENABLE_VULKAN)
// With extension flag and extension directive, compiling using TranslatorVulkan succeeds.
TEST_P(EXTClipCullDistanceForVertexShaderTest, CompileSucceedsVulkan)
{
    SetExtensionEnable(true);

    mResources.MaxClipDistances                = 8;
    mResources.MaxCullDistances                = 8;
    mResources.MaxCombinedClipAndCullDistances = 8;

    InitializeCompiler(SH_SPIRV_VULKAN_OUTPUT);
    EXPECT_TRUE(TestShaderCompile(EXTPragma));
    EXPECT_FALSE(TestShaderCompile(""));
    EXPECT_TRUE(TestShaderCompile(EXTPragma));
}
#endif

// Extension flag is required to compile properly. Expect failure when it is
// not present.
TEST_P(EXTClipCullDistanceForFragmentShaderTest, CompileFailsWithoutExtension)
{
    SetExtensionEnable(false);
    InitializeCompiler();
    EXPECT_FALSE(TestShaderCompile(EXTPragma));
}

// Extension directive is required to compile properly. Expect failure when
// it is not present.
TEST_P(EXTClipCullDistanceForFragmentShaderTest, CompileFailsWithExtensionWithoutPragma)
{
    SetExtensionEnable(true);
    InitializeCompiler();
    EXPECT_FALSE(TestShaderCompile(""));
}

#if defined(ANGLE_ENABLE_VULKAN)
// With extension flag and extension directive, compiling using TranslatorVulkan succeeds.
//
// Test is disabled due to translation bug.  http://anglebug.com/5747
TEST_P(EXTClipCullDistanceForFragmentShaderTest, DISABLED_CompileSucceedsVulkan)
{
    SetExtensionEnable(true);

    mResources.MaxClipDistances                = 8;
    mResources.MaxCullDistances                = 8;
    mResources.MaxCombinedClipAndCullDistances = 8;

    InitializeCompiler(SH_SPIRV_VULKAN_OUTPUT);
    EXPECT_TRUE(TestShaderCompile(EXTPragma));
    EXPECT_FALSE(TestShaderCompile(""));
    EXPECT_TRUE(TestShaderCompile(EXTPragma));
}

class EXTClipCullDistanceForVertexShaderCompileFailureTest
    : public EXTClipCullDistanceForVertexShaderTest
{};

class EXTClipCullDistanceForFragmentShaderCompileFailureTest
    : public EXTClipCullDistanceForFragmentShaderTest
{};

TEST_P(EXTClipCullDistanceForVertexShaderCompileFailureTest, CompileFails)
{
    SetExtensionEnable(true);

    mResources.MaxClipDistances                = 8;
    mResources.MaxCullDistances                = 8;
    mResources.MaxCombinedClipAndCullDistances = 8;

    InitializeCompiler(SH_SPIRV_VULKAN_OUTPUT);
    EXPECT_FALSE(TestShaderCompile(EXTPragma));
}

TEST_P(EXTClipCullDistanceForFragmentShaderCompileFailureTest, CompileFails)
{
    SetExtensionEnable(true);

    mResources.MaxClipDistances                = 8;
    mResources.MaxCullDistances                = 8;
    mResources.MaxCombinedClipAndCullDistances = 8;

    InitializeCompiler(SH_SPIRV_VULKAN_OUTPUT);
    EXPECT_FALSE(TestShaderCompile(EXTPragma));
}
#endif

INSTANTIATE_TEST_SUITE_P(CorrectESSL300Shaders,
                         EXTClipCullDistanceForVertexShaderTest,
                         Combine(Values(SH_GLES3_SPEC),
                                 Values(sh::ESSLVersion300),
                                 Values(VertexShaderCompileSucceeds1,
                                        VertexShaderCompileSucceeds2)));

INSTANTIATE_TEST_SUITE_P(CorrectESSL300Shaders,
                         EXTClipCullDistanceForFragmentShaderTest,
                         Combine(Values(SH_GLES3_SPEC),
                                 Values(sh::ESSLVersion300),
                                 Values(FragmentShaderCompileSucceeds1,
                                        FragmentShaderCompileSucceeds2)));

// The corresponding TEST_Ps are defined only when ANGLE_ENABLE_VULKAN is
// defined.
#if defined(ANGLE_ENABLE_VULKAN)
INSTANTIATE_TEST_SUITE_P(IncorrectESSL100Shaders,
                         EXTClipCullDistanceForVertexShaderCompileFailureTest,
                         Combine(Values(SH_GLES2_SPEC),
                                 Values(sh::ESSLVersion100),
                                 Values(VertexShaderCompileFails5, VertexShaderCompileFailes6)));

INSTANTIATE_TEST_SUITE_P(IncorrectESSL300Shaders,
                         EXTClipCullDistanceForVertexShaderCompileFailureTest,
                         Combine(Values(SH_GLES3_SPEC),
                                 Values(sh::ESSLVersion300),
                                 Values(VertexShaderCompileFails1,
                                        VertexShaderCompileFails2,
                                        VertexShaderCompileFails3,
                                        VertexShaderCompileFails4)));

INSTANTIATE_TEST_SUITE_P(IncorrectESSL300Shaders,
                         EXTClipCullDistanceForFragmentShaderCompileFailureTest,
                         Combine(Values(SH_GLES3_SPEC),
                                 Values(sh::ESSLVersion300),
                                 Values(FragmentShaderCompileFails1,
                                        FragmentShaderCompileFails2,
                                        FragmentShaderCompileFails3,
                                        FragmentShaderCompileFails4,
                                        FragmentShaderCompileFails5)));
#endif

}  // anonymous namespace
