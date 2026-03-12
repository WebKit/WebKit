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
};

class GLSLOutputGLSLTest : public GLSLOutputTest
{};

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
}  // namespace

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLOutputGLSLTest);
ANGLE_INSTANTIATE_TEST(GLSLOutputGLSLTest, ES2_OPENGL(), ES2_OPENGLES());

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLSLOutputGLSLVerifyIRUseTest);
ANGLE_INSTANTIATE_TEST(GLSLOutputGLSLVerifyIRUseTest,
                       ES2_OPENGL(),
                       ES2_OPENGLES(),
                       ES2_OPENGL().disable(Feature::UseIr),
                       ES2_OPENGLES().disable(Feature::UseIr));
