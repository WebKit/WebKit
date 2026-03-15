//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "test_utils/CompilerTest.h"

#include "test_utils/angle_test_configs.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{
class GLSLOutputTest : public CompilerTest
{
  protected:
    GLSLOutputTest() {}

    // Helper to create a shader
    void compileShader(GLenum shaderType, const char *shaderSource)
    {
        const CompiledShader &shader = compile(shaderType, shaderSource);
        EXPECT_TRUE(shader.success());
    }

    // Check that the output contains (or not) the expected substring.
    void verifyIsInTranslation(GLenum shaderType, const char *expect)
    {
        EXPECT_TRUE(getCompiledShader(shaderType).verifyInTranslatedSource(expect)) << expect;
    }
    void verifyIsNotInTranslation(GLenum shaderType, const char *expect)
    {
        EXPECT_TRUE(getCompiledShader(shaderType).verifyNotInTranslatedSource(expect)) << expect;
    }
    void verifyCountInTranslation(GLenum shaderType, const char *expect, size_t expectCount)
    {
        EXPECT_TRUE(
            getCompiledShader(shaderType).verifyCountInTranslatedSource(expect, expectCount))
            << expectCount << "x " << expect;
    }
};

class GLSLOutputGLSLTest : public GLSLOutputTest
{};

class GLSLOutputGLSLTest_ES3 : public GLSLOutputTest
{};

class WebGLGLSLOutputGLSLTest : public GLSLOutputGLSLTest
{
  protected:
    WebGLGLSLOutputGLSLTest() { setWebGLCompatibilityEnabled(true); }
};

class WebGL2GLSLOutputGLSLTest : public GLSLOutputGLSLTest_ES3
{
  protected:
    WebGL2GLSLOutputGLSLTest() { setWebGLCompatibilityEnabled(true); }
};

// Verifies that without explicitly enabling GL_EXT_draw_buffers extension in the shader, no
// broadcast emulation.
TEST_P(GLSLOutputGLSLTest, FragColorNoBroadcast)
{
    constexpr char kFS[] = R"(void main()
{
    gl_FragColor = vec4(1, 0, 0, 0);
})";
    compileShader(GL_FRAGMENT_SHADER, kFS);
    verifyIsInTranslation(GL_FRAGMENT_SHADER, "gl_FragColor");
    verifyIsNotInTranslation(GL_FRAGMENT_SHADER, "gl_FragData[0]");
    verifyIsNotInTranslation(GL_FRAGMENT_SHADER, "gl_FragData[1]");
}

// Verifies that with explicitly enabling GL_EXT_draw_buffers extension
// in the shader, broadcast is emualted by replacing gl_FragColor with gl_FragData.
TEST_P(GLSLOutputGLSLTest, FragColorBroadcast)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_draw_buffers"));

    constexpr char kFS[] = R"(#extension GL_EXT_draw_buffers : require
void main()
{
    gl_FragColor = vec4(1, 0, 0, 0);
})";
    compileShader(GL_FRAGMENT_SHADER, kFS);
    verifyIsNotInTranslation(GL_FRAGMENT_SHADER, "gl_FragColor");
    verifyIsInTranslation(GL_FRAGMENT_SHADER, "gl_FragData[0]");
    verifyIsInTranslation(GL_FRAGMENT_SHADER, "gl_FragData[1]");
}

// Verifies that with explicitly enabling GL_EXT_draw_buffers extension
// in the shader with an empty main(), nothing happens.
TEST_P(GLSLOutputGLSLTest, EmptyMain)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_draw_buffers"));

    constexpr char kFS[] = R"(#extension GL_EXT_draw_buffers : require
void main()
{
})";
    compileShader(GL_FRAGMENT_SHADER, kFS);
    verifyIsNotInTranslation(GL_FRAGMENT_SHADER, "gl_FragColor");
    verifyIsNotInTranslation(GL_FRAGMENT_SHADER, "gl_FragData[0]");
    verifyIsNotInTranslation(GL_FRAGMENT_SHADER, "gl_FragData[1]");
}

// Test the initialization of output variables with various qualifiers in a vertex shader.
TEST_P(WebGL2GLSLOutputGLSLTest, OutputAllQualifiers)
{
    constexpr char kVS[] = R"(#version 300 es
precision mediump float;
precision lowp int;
out vec4 out1;
flat out int out2;
centroid out float out3;
smooth out float out4;
void main() {
  out1.x += 0.0001;
  out2 += 1;
  out3 += 0.0001;
  out4 += 0.0001;
})";
    compileShader(GL_VERTEX_SHADER, kVS);
    verifyIsInTranslation(GL_VERTEX_SHADER, "gl_Position = vec4(0.0, 0.0, 0.0, 0.0)");
    verifyIsInTranslation(GL_VERTEX_SHADER, "_uout1 = vec4(0.0, 0.0, 0.0, 0.0)");
    verifyIsInTranslation(GL_VERTEX_SHADER, "_uout2 = 0");
    verifyIsInTranslation(GL_VERTEX_SHADER, "_uout3 = 0.0");
    verifyIsInTranslation(GL_VERTEX_SHADER, "_uout4 = 0.0");
}

// Test the initialization of an output array in a vertex shader.
TEST_P(WebGL2GLSLOutputGLSLTest, OutputArray)
{
    constexpr char kVS[] = R"(#version 300 es
precision mediump float;
out float out1[2];
void main() {
  out1[0] += 0.0001;
})";
    compileShader(GL_VERTEX_SHADER, kVS);
    verifyIsInTranslation(GL_VERTEX_SHADER, "_uout1[0] = 0.0");
    verifyIsInTranslation(GL_VERTEX_SHADER, "_uout1[1] = 0.0");
}

// Test the initialization of a struct output variable in a vertex shader.
TEST_P(WebGL2GLSLOutputGLSLTest, OutputStruct)
{
    constexpr char kVS[] = R"(#version 300 es
precision mediump float;
struct MyS{
   float a;
   float b;
};
out MyS out1;
void main() {
  out1.a += 0.0001;
})";
    compileShader(GL_VERTEX_SHADER, kVS);
    if (getEGLWindow()->isFeatureEnabled(Feature::UseIr))
    {
        verifyIsInTranslation(GL_VERTEX_SHADER, "_uout1._ua = 0.0");
        verifyIsInTranslation(GL_VERTEX_SHADER, "_uout1._ub = 0.0");
    }
    else
    {
        verifyIsInTranslation(GL_VERTEX_SHADER, "_uout1 = _uMyS(");
    }
}

// Test the initialization of a varying variable in an ESSL1 vertex shader.
TEST_P(WebGL2GLSLOutputGLSLTest, OutputFromESSL1Shader)
{
    constexpr char kVS[] = R"(precision mediump float;
varying vec4 out1;
void main() {
  out1.x += 0.0001;
})";
    compileShader(GL_VERTEX_SHADER, kVS);
    verifyIsInTranslation(GL_VERTEX_SHADER, "gl_Position = vec4(0.0, 0.0, 0.0, 0.0)");
    verifyIsInTranslation(GL_VERTEX_SHADER, "_uout1 = vec4(0.0, 0.0, 0.0, 0.0)");
}

// Test the initialization of output variables in a fragment shader.
TEST_P(WebGL2GLSLOutputGLSLTest, FragmentOutput)
{
    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
out vec4 out1;
void main() {
  out1.x += 0.0001;
})";
    compileShader(GL_FRAGMENT_SHADER, kFS);
    verifyIsInTranslation(GL_FRAGMENT_SHADER, "_uout1 = vec4(0.0, 0.0, 0.0, 0.0)");
}

// Test the initialization of gl_FragData in a WebGL2 ESSL1 fragment shader.  Only writes to
// gl_FragData[0] should be found.
TEST_P(WebGL2GLSLOutputGLSLTest, FragData)
{
    constexpr char kFS[] = R"(precision mediump float;
void main() {
   gl_FragData[0] = vec4(1.);
})";
    compileShader(GL_FRAGMENT_SHADER, kFS);
    verifyIsInTranslation(GL_FRAGMENT_SHADER, "gl_FragData[0] = vec4(0.0, 0.0, 0.0, 0.0)");
    verifyIsNotInTranslation(GL_FRAGMENT_SHADER, "gl_FragData[1]");
}

// Test the initialization of gl_FragData in a WebGL1 ESSL1 fragment shader.  Only writes to
// gl_FragData[0] should be found.
TEST_P(WebGLGLSLOutputGLSLTest, FragData)
{
    constexpr char kFS[] = R"(precision mediump float;
void main() {
   gl_FragData[0] = vec4(1.);
})";
    compileShader(GL_FRAGMENT_SHADER, kFS);
    verifyIsInTranslation(GL_FRAGMENT_SHADER, "gl_FragData[0] = vec4(0.0, 0.0, 0.0, 0.0)");
    verifyIsNotInTranslation(GL_FRAGMENT_SHADER, "gl_FragData[1]");
}

// Test the initialization of gl_FragData in a WebGL1 ESSL1 fragment shader with GL_EXT_draw_buffers
// enabled.  All attachment slots should be initialized.
TEST_P(WebGLGLSLOutputGLSLTest, FragDataWithDrawBuffersExtEnabled)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_draw_buffers"));

    constexpr char kFS[] = R"(#extension GL_EXT_draw_buffers : enable
precision mediump float;
void main() {
   gl_FragData[0] = vec4(1.);
})";
    compileShader(GL_FRAGMENT_SHADER, kFS);
    verifyIsInTranslation(GL_FRAGMENT_SHADER, "gl_FragData[0] = vec4(0.0, 0.0, 0.0, 0.0)");
    verifyIsInTranslation(GL_FRAGMENT_SHADER, "gl_FragData[1] = vec4(0.0, 0.0, 0.0, 0.0)");
}

// Test that gl_Position is initialized once in case it is not statically used and both
// initOutputVariables (by webgl) and initGLPosition (by webgl, but also the GL backend) flags are
// set.
TEST_P(WebGL2GLSLOutputGLSLTest, InitGLPositionWhenNotStaticallyUsed)
{
    constexpr char kVS[] = R"(#version 300 es
precision highp float;
void main() {
})";
    compileShader(GL_VERTEX_SHADER, kVS);
    verifyCountInTranslation(GL_VERTEX_SHADER, "gl_Position = vec4(0.0, 0.0, 0.0, 0.0)", 1);
}

// Test that gl_Position is initialized once in case it is statically used and both
// initOutputVariables (by webgl) and initGLPosition (by webgl, but also the GL backend) flags are
// set.
TEST_P(WebGL2GLSLOutputGLSLTest, InitGLPositionOnceWhenStaticallyUsed)
{
    constexpr char kVS[] = R"(#version 300 es
precision highp float;
void main() {
    gl_Position = vec4(1.0);
})";
    compileShader(GL_VERTEX_SHADER, kVS);
    verifyCountInTranslation(GL_VERTEX_SHADER, "gl_Position = vec4(0.0, 0.0, 0.0, 0.0)", 1);
}

class GLSLOutputGLSLTest_InitShaderVariables : public GLSLOutputGLSLTest
{};

// Mirrors ClipDistanceTest.ThreeClipDistancesRedeclared
TEST_P(GLSLOutputGLSLTest_InitShaderVariables, RedeclareClipDistance)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_APPLE_clip_distance"));

    constexpr char kVS[] = R"(#extension GL_APPLE_clip_distance : require

varying highp float gl_ClipDistance[3];

void computeClipDistances(in vec4 position, in vec4 plane[3])
{
    gl_ClipDistance[0] = dot(position, plane[0]);
    gl_ClipDistance[1] = dot(position, plane[1]);
    gl_ClipDistance[2] = dot(position, plane[2]);
}

uniform vec4 u_plane[3];

attribute vec2 a_position;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);

    computeClipDistances(gl_Position, u_plane);
})";
    compileShader(GL_VERTEX_SHADER, kVS);
    verifyIsInTranslation(GL_VERTEX_SHADER, "gl_Position = vec4(0.0, 0.0, 0.0, 0.0)");
    verifyIsInTranslation(GL_VERTEX_SHADER, "gl_ClipDistance[0] = 0.0");
    verifyIsInTranslation(GL_VERTEX_SHADER, "gl_ClipDistance[1] = 0.0");
    verifyIsInTranslation(GL_VERTEX_SHADER, "gl_ClipDistance[2] = 0.0");
}

class GLSLOutputGLSLVerifyIRUseTest : public GLSLOutputGLSLTest
{};

// A basic test that makes sure the `useIr` feature is actually effective.
TEST_P(GLSLOutputGLSLVerifyIRUseTest, Basic)
{
    constexpr char kFS[] = R"(void main()
{
})";
    compileShader(GL_FRAGMENT_SHADER, kFS);
    // With AST, implicit `return` remains implicit.  With IR, every block ends in a branch, so the
    // `return` is explicit.
    if (getEGLWindow()->isFeatureEnabled(Feature::UseIr))
    {
        verifyIsInTranslation(GL_FRAGMENT_SHADER, "return");
    }
    else
    {
        verifyIsNotInTranslation(GL_FRAGMENT_SHADER, "return");
    }
}

class GLSLOutputMSLTest_EnsureLoopForwardProgress : public GLSLOutputTest
{};

// Test that loopForwardProgress() is not inserted when the for loop is obviously not an infinite
// loop.
TEST_P(GLSLOutputMSLTest_EnsureLoopForwardProgress, FiniteBasicFor)
{
    constexpr char kFS[] = R"(#version 300 es
void main() {
    for (highp int i = 0; i < 100; ++i) { }
})";
    compileShader(GL_FRAGMENT_SHADER, kFS);
    // The AST transformation (EnsureLoopForwardProgress ) expects a |for| loop, but the IR changes
    // it to |while| before that's run.  So when the IR is used, the test would fail as the
    // transformaiton is unable to correctly detect finite loops.
    //
    // Once the transformation is ported to the IR, the test verification can be enabled.
    if (!getEGLWindow()->isFeatureEnabled(Feature::UseIr))
    {
        verifyIsNotInTranslation(GL_FRAGMENT_SHADER, "loopForwardProgress");
    }
}

// Test that loopForwardProgress() is inserted when the for loop is an infinite loop.
TEST_P(GLSLOutputMSLTest_EnsureLoopForwardProgress, InfiniteFor)
{
    constexpr char kFS[] = R"(#version 300 es
void main() {
    for (highp int i = 0; i < 100; i++) { i = 0; }
})";
    compileShader(GL_FRAGMENT_SHADER, kFS);
    if (!getEGLWindow()->isFeatureEnabled(Feature::UseIr))
    {
        // One occurrence for defining |loopForwardProgress()|, and one call in the loop.
        verifyCountInTranslation(GL_FRAGMENT_SHADER, "loopForwardProgress", 1 + 1);
    }
}

// Test that loopForwardProgress() is inserted when nested for loops are infinite loops.
TEST_P(GLSLOutputMSLTest_EnsureLoopForwardProgress, InfiniteNestedFor)
{
    constexpr char kFS[] = R"(#version 300 es
void main() {
    for (highp int i = 0; i < 100; i++)
    {
        for (highp int j = 0; j < 100; j++)
        {
            j = 0;
        }
        i = 0;
    }
})";
    compileShader(GL_FRAGMENT_SHADER, kFS);
    if (!getEGLWindow()->isFeatureEnabled(Feature::UseIr))
    {
        // One occurrence for defining |loopForwardProgress()|, and one call in each loop.
        verifyCountInTranslation(GL_FRAGMENT_SHADER, "loopForwardProgress", 2 + 1);
    }
}

// Test that loopForwardProgress() is not inserted when the for loop is not an infinite loop,
// testing various tricky loops.
TEST_P(GLSLOutputMSLTest_EnsureLoopForwardProgress, FiniteFors)
{
    const char kShaderPrefix[] = R"(#version 300 es
precision highp int;
uniform int a;
uniform uint b;
void main() {

)";
    const char kShaderSuffix[] = "}\n";
    const char *kTests[]{"int i = 101; for (; i < 10; i++) { }",
                         "int i = 101; for (; i < 10; i+=1) { }",
                         "int i = 101; for (; i < 10; i-=1) { }",
                         "for (int i = 0; i < 10; i++) { }",
                         "for (int i = 0; i < a; i++) { }",
                         "for (int i = 0; i < 100000/2; ++i) { }",
                         "for (uint i = 0u; i < 10u; i++) { }",
                         "for (uint i = 0u; i < b; i++) { }",
                         "for (uint i = 0u; i < 100000u/2u; ++i) { }",
                         "for (uint i = 0u; i < 4294967295u; ++i) { }",
                         "for (uint i = 10u; i > 1u+3u ; --i) { }",
                         "const int z = 7; for (int i = 0; i < z; i++) { }",
                         "for (int i = 0; i < 10; i++) { for (int j = 0; j < 1000; ++j) { }}"};

    for (const char *test : kTests)
    {
        std::string shader = (std::stringstream() << kShaderPrefix << test << kShaderSuffix).str();
        compileShader(GL_FRAGMENT_SHADER, shader.c_str());
        if (!getEGLWindow()->isFeatureEnabled(Feature::UseIr))
        {
            verifyIsNotInTranslation(GL_FRAGMENT_SHADER, "loopForwardProgress");
        }
    }
}

// Test that loopForwardProgress() is inserted when the for loop is an infinite loop,
// testing various tricky loops.
TEST_P(GLSLOutputMSLTest_EnsureLoopForwardProgress, InfiniteFors)
{
    const char kShaderPrefix[] = R"(#version 300 es
precision highp int;
uniform int a;
uniform uint b;
void main() {

)";
    const char kShaderSuffix[] = "}\n";
    const char *kTests[]{"for (;;) { }",
                         "for (bool b = true; b; b = false) { }",
                         "for (int i = 0; i < 10;) { }",
                         "int i = 101; for (; i < 10; i+=2) { }",
                         "int i = 101; for (; i < 10; i-=2) { }",
                         "int z = 7; for (int i = 0; i < z; i++) { }",
                         "for (int i = 0; i < 10; i++) { i++; }",
                         "for (int i = 0; i < 10;) { i++; }",
                         "for (int i = 0; i < a/2; i++) { }",
                         "for (int i = 0; float(i) < 10e10; ++i) { }",
                         "for (int i = 0; i < 10; i++) { for (int j = 0; j < 1000; ++i) { }}",
                         "for (int i = 0; i != 1; i+=2) { }"};

    for (const char *test : kTests)
    {
        std::string shader = (std::stringstream() << kShaderPrefix << test << kShaderSuffix).str();
        compileShader(GL_FRAGMENT_SHADER, shader.c_str());
        if (!getEGLWindow()->isFeatureEnabled(Feature::UseIr))
        {
            verifyIsInTranslation(GL_FRAGMENT_SHADER, "loopForwardProgress");
        }
    }
}
}  // namespace

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLOutputGLSLTest);
ANGLE_INSTANTIATE_TEST(GLSLOutputGLSLTest, ES2_OPENGL(), ES2_OPENGLES());

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLOutputGLSLTest_ES3);
ANGLE_INSTANTIATE_TEST(GLSLOutputGLSLTest_ES3, ES3_OPENGL(), ES3_OPENGLES());

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(WebGLGLSLOutputGLSLTest);
ANGLE_INSTANTIATE_TEST(WebGLGLSLOutputGLSLTest, ES2_OPENGL(), ES2_OPENGLES());

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(WebGL2GLSLOutputGLSLTest);
ANGLE_INSTANTIATE_TEST(WebGL2GLSLOutputGLSLTest, ES3_OPENGL(), ES3_OPENGLES());

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLOutputGLSLTest_InitShaderVariables);
ANGLE_INSTANTIATE_TEST(GLSLOutputGLSLTest_InitShaderVariables,
                       ES2_OPENGL().enable(Feature::ForceInitShaderVariables),
                       ES2_OPENGLES().enable(Feature::ForceInitShaderVariables));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLOutputGLSLVerifyIRUseTest);
ANGLE_INSTANTIATE_TEST(GLSLOutputGLSLVerifyIRUseTest,
                       ES2_OPENGL(),
                       ES2_OPENGLES(),
                       ES2_OPENGL().disable(Feature::UseIr),
                       ES2_OPENGLES().disable(Feature::UseIr));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLOutputMSLTest_EnsureLoopForwardProgress);
ANGLE_INSTANTIATE_TEST(GLSLOutputMSLTest_EnsureLoopForwardProgress,
                       ES3_METAL().enable(Feature::EnsureLoopForwardProgress));
