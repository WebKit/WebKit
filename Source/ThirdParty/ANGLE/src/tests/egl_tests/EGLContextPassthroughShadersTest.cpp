//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLContextPassthroughShadersTest.cpp:
//  Tests of the EGL_ANGLE_create_context_passthrough_shaders extension.
//

#include <gtest/gtest.h>

#include "GLES2/gl2.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/angle_test_instantiate.h"
#include "test_utils/angle_test_platform.h"
#include "util/gles_loader_autogen.h"

using namespace angle;

class EGLContextPassthroughShadersTest : public ANGLETest<>
{
  public:
    EGLContextPassthroughShadersTest() : mDisplay(EGL_NO_DISPLAY) {}

    void testSetUp() override
    {
        EGLAttrib dispattrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, GetParam().getRenderer(), EGL_NONE};
        mDisplay              = eglGetPlatformDisplay(GetEglPlatform(),
                                                      reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
        EXPECT_TRUE(mDisplay != EGL_NO_DISPLAY);
        EXPECT_EGL_TRUE(eglInitialize(mDisplay, nullptr, nullptr) != EGL_FALSE);

        EGLint attribs[] = {EGL_RED_SIZE,
                            8,
                            EGL_GREEN_SIZE,
                            8,
                            EGL_BLUE_SIZE,
                            8,
                            EGL_ALPHA_SIZE,
                            8,
                            EGL_RENDERABLE_TYPE,
                            EGL_OPENGL_ES2_BIT,
                            EGL_SURFACE_TYPE,
                            EGL_PBUFFER_BIT,
                            EGL_NONE};

        EGLint count = 0;
        EXPECT_EGL_TRUE(eglChooseConfig(mDisplay, attribs, &mConfig, 1, &count));
        ANGLE_SKIP_TEST_IF(mConfig == EGL_NO_CONFIG_KHR);
        EXPECT_GT(count, 0);

        EGLint pBufferAttribs[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
        mSurface                = eglCreatePbufferSurface(mDisplay, mConfig, pBufferAttribs);
        EXPECT_NE(mSurface, EGL_NO_SURFACE);
    }

    void testTearDown() override
    {
        if (mDisplay != EGL_NO_DISPLAY)
        {
            eglTerminate(mDisplay);
            eglReleaseThread();
            mDisplay = EGL_NO_DISPLAY;
        }
        ASSERT_EGL_SUCCESS() << "Error during test TearDown";
    }

    bool supportsPassthroughShadersExtension()
    {
        return IsEGLDisplayExtensionEnabled(mDisplay,
                                            "EGL_ANGLE_create_context_passthrough_shaders");
    }

    EGLDisplay mDisplay;
    EGLConfig mConfig;
    EGLSurface mSurface;
};

// Test creating a context with passthrough shaders enabled and verify by querying translated
// shaders source
TEST_P(EGLContextPassthroughShadersTest, CreateContext)
{
    ANGLE_SKIP_TEST_IF(!supportsPassthroughShadersExtension());

    EGLint ctxAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_CONTEXT_PASSTHROUGH_SHADERS_ANGLE,
                           EGL_TRUE, EGL_NONE};
    EGLContext context  = eglCreateContext(mDisplay, mConfig, nullptr, ctxAttribs);
    EXPECT_NE(context, EGL_NO_CONTEXT);

    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, mSurface, mSurface, context));

    constexpr const char kFragmentShader[] = R"(
            precision highp float;
            uniform sampler2D tex;
            varying vec2 texcoord;

            #define TEST_MACRO_THAT_WOULD_BE_REMOVED

            void main()
            {
                gl_FragColor = texture2D(tex, texcoord);
            }
        )";
    GLuint shader                          = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);

    EXPECT_TRUE(EnsureGLExtensionEnabled("GL_ANGLE_translated_shader_source"));
    std::array<char, std::size(kFragmentShader) + 1> translatedSourceBuffer;
    glGetTranslatedShaderSourceANGLE(shader, static_cast<GLsizei>(translatedSourceBuffer.size()),
                                     nullptr, translatedSourceBuffer.data());
    EXPECT_EQ(std::string(kFragmentShader), std::string(translatedSourceBuffer.data()));
}

// Regression test for a Skia shader which had assertion failures in CollectVariables
TEST_P(EGLContextPassthroughShadersTest, ShaderRegressionTest)
{
    ANGLE_SKIP_TEST_IF(!supportsPassthroughShadersExtension());

    EGLint ctxAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_PASSTHROUGH_SHADERS_ANGLE,
                           EGL_TRUE, EGL_NONE};
    EGLContext context  = eglCreateContext(mDisplay, mConfig, nullptr, ctxAttribs);
    EXPECT_NE(context, EGL_NO_CONTEXT);

    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, mSurface, mSurface, context));

    constexpr const char kShader[] = R"(#version 300 es

precision mediump float;
precision mediump sampler2D;
const highp float PRECISION = 4.0;
const highp float MAX_FIXED_RESOLVE_LEVEL = 5.0;
const highp float MAX_FIXED_SEGMENTS = 32.0;
uniform highp vec4 sk_RTAdjust;
uniform highp vec4 uaffineMatrix_S0;
uniform highp vec2 utranslate_S0;
in highp vec2 resolveLevel_and_idx;
in highp vec4 p01;
in highp vec4 p23;
in highp vec2 fanPointAttrib;
highp float wangs_formula_max_fdiff_p2_ff2f2f2f2f22(highp vec2 p0, highp vec2 p1, highp vec2 p2, highp vec2 p3, highp mat2 matrix) {
highp vec2 d0 = matrix * (((vec2(-2.0)) * (p1) + (p2)) + p0);
highp vec2 d1 = matrix * (((vec2(-2.0)) * (p2) + (p3)) + p1);
return max(dot(d0, d0), dot(d1, d1));
}
highp float wangs_formula_conic_p2_fff2f2f2f(highp float _precision_, highp vec2 p0, highp vec2 p1, highp vec2 p2, highp float w) {
highp vec2 C = (min(min(p0, p1), p2) + max(max(p0, p1), p2)) * 0.5;
p0 -= C;
p1 -= C;
p2 -= C;
highp float m = sqrt(max(max(dot(p0, p0), dot(p1, p1)), dot(p2, p2)));
highp vec2 dp = ((vec2(-2.0 * w)) * (p1) + (p0)) + p2;
highp float dw = abs(((-2.0) * (w) + (2.0)));
highp float rp_minus_1 = max(0.0, ((m) * (_precision_) + (-1.0)));
highp float numer = length(dp) * _precision_ + rp_minus_1 * dw;
highp float denom = 4.0 * min(w, 1.0);
return numer / denom;
}
void main() {
highp mat2 AFFINE_MATRIX = mat2(uaffineMatrix_S0.xy, uaffineMatrix_S0.zw);
highp vec2 TRANSLATE = utranslate_S0;
highp float resolveLevel = resolveLevel_and_idx.x;
highp float idxInResolveLevel = resolveLevel_and_idx.y;
highp vec2 localcoord;
if (resolveLevel < 0.0) {
localcoord = fanPointAttrib;
} else {
if (isinf(p23.z)) {
localcoord = resolveLevel != 0.0 ? p01.zw : (idxInResolveLevel != 0.0 ? p23.xy : p01.xy);
} else {
highp vec2 p0 = p01.xy;
highp vec2 p1 = p01.zw;
highp vec2 p2 = p23.xy;
highp vec2 p3 = p23.zw;
highp float w = -1.0;
highp float maxResolveLevel;
if (isinf(p23.w)) {
w = p3.x;
highp float _0_n2 = wangs_formula_conic_p2_fff2f2f2f(PRECISION, AFFINE_MATRIX * p0, AFFINE_MATRIX * p1, AFFINE_MATRIX * p2, w);
maxResolveLevel = ceil(log2(max(_0_n2, 1.0)) * 0.5);
p1 *= w;
p3 = p2;
} else {
highp float _1_m = wangs_formula_max_fdiff_p2_ff2f2f2f2f22(p0, p1, p2, p3, AFFINE_MATRIX);
maxResolveLevel = ceil(log2(max(9.0 * _1_m, 1.0)) * 0.25);
}
if (resolveLevel > maxResolveLevel) {
idxInResolveLevel = floor(idxInResolveLevel * exp2(maxResolveLevel - resolveLevel));
resolveLevel = maxResolveLevel;
}
highp float fixedVertexID = floor(0.5 + idxInResolveLevel * exp2(MAX_FIXED_RESOLVE_LEVEL - resolveLevel));
if (0.0 < fixedVertexID && fixedVertexID < MAX_FIXED_SEGMENTS) {
highp float T = fixedVertexID * 0.03125;
highp vec2 ab = mix(p0, p1, T);
highp vec2 bc = mix(p1, p2, T);
highp vec2 cd = mix(p2, p3, T);
highp vec2 abc = mix(ab, bc, T);
highp vec2 bcd = mix(bc, cd, T);
highp vec2 abcd = mix(abc, bcd, T);
highp float u = mix(1.0, w, T);
highp float v = (w + 1.0) - u;
highp float uv = mix(u, v, T);
localcoord = w < 0.0 ? abcd : abc / uv;
} else {
localcoord = fixedVertexID == 0.0 ? p0 : p3;
}
}
}
highp vec2 vertexpos = AFFINE_MATRIX * localcoord + TRANSLATE;
gl_Position = vec4(vertexpos, 0.0, 1.0);
gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * sk_RTAdjust.yw, 0.0, gl_Position.w);
}
)";
    GLuint shader                  = CompileShader(GL_VERTEX_SHADER, kShader);
    EXPECT_NE(0u, shader);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLContextPassthroughShadersTest);
ANGLE_INSTANTIATE_TEST(EGLContextPassthroughShadersTest,
                       WithNoFixture(ES2_D3D9()),
                       WithNoFixture(ES2_D3D11()),
                       WithNoFixture(ES2_OPENGL()),
                       WithNoFixture(ES2_OPENGLES()),
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_D3D11()),
                       WithNoFixture(ES3_OPENGL()),
                       WithNoFixture(ES3_OPENGLES()),
                       WithNoFixture(ES3_VULKAN()));
