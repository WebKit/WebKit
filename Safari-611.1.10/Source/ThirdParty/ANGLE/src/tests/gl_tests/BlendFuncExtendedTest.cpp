//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BlendFuncExtendedTest
//   Test EXT_blend_func_extended

#include "test_utils/ANGLETest.h"

#include "util/shader_utils.h"

#include <algorithm>
#include <cmath>
#include <fstream>

using namespace angle;

namespace
{

// Partial implementation of weight function for GLES 2 blend equation that
// is dual-source aware.
template <int factor, int index>
float Weight(const float /*dst*/[4], const float src[4], const float src1[4])
{
    if (factor == GL_SRC_COLOR)
        return src[index];
    if (factor == GL_SRC_ALPHA)
        return src[3];
    if (factor == GL_SRC1_COLOR_EXT)
        return src1[index];
    if (factor == GL_SRC1_ALPHA_EXT)
        return src1[3];
    if (factor == GL_ONE_MINUS_SRC1_COLOR_EXT)
        return 1.0f - src1[index];
    if (factor == GL_ONE_MINUS_SRC1_ALPHA_EXT)
        return 1.0f - src1[3];
    return 0.0f;
}

GLubyte ScaleChannel(float weight)
{
    return static_cast<GLubyte>(std::floor(std::max(0.0f, std::min(1.0f, weight)) * 255.0f));
}

// Implementation of GLES 2 blend equation that is dual-source aware.
template <int RGBs, int RGBd, int As, int Ad>
void BlendEquationFuncAdd(const float dst[4],
                          const float src[4],
                          const float src1[4],
                          angle::GLColor *result)
{
    float r[4];
    r[0] = src[0] * Weight<RGBs, 0>(dst, src, src1) + dst[0] * Weight<RGBd, 0>(dst, src, src1);
    r[1] = src[1] * Weight<RGBs, 1>(dst, src, src1) + dst[1] * Weight<RGBd, 1>(dst, src, src1);
    r[2] = src[2] * Weight<RGBs, 2>(dst, src, src1) + dst[2] * Weight<RGBd, 2>(dst, src, src1);
    r[3] = src[3] * Weight<As, 3>(dst, src, src1) + dst[3] * Weight<Ad, 3>(dst, src, src1);

    result->R = ScaleChannel(r[0]);
    result->G = ScaleChannel(r[1]);
    result->B = ScaleChannel(r[2]);
    result->A = ScaleChannel(r[3]);
}

void CheckPixels(GLint x,
                 GLint y,
                 GLsizei width,
                 GLsizei height,
                 GLint tolerance,
                 const angle::GLColor &color)
{
    for (GLint yy = 0; yy < height; ++yy)
    {
        for (GLint xx = 0; xx < width; ++xx)
        {
            const auto px = x + xx;
            const auto py = y + yy;
            EXPECT_PIXEL_COLOR_NEAR(px, py, color, 1);
        }
    }
}

const GLuint kWidth  = 100;
const GLuint kHeight = 100;

class EXTBlendFuncExtendedTest : public ANGLETest
{};

class EXTBlendFuncExtendedTestES3 : public ANGLETest
{};

class EXTBlendFuncExtendedDrawTest : public ANGLETest
{
  protected:
    EXTBlendFuncExtendedDrawTest() : mProgram(0)
    {
        setWindowWidth(kWidth);
        setWindowHeight(kHeight);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testSetUp() override
    {
        glGenBuffers(1, &mVBO);
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);

        static const float vertices[] = {
            1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteBuffers(1, &mVBO);
        if (mProgram)
        {
            glDeleteProgram(mProgram);
        }

        ASSERT_GL_NO_ERROR();
    }

    void makeProgram(const char *vertSource, const char *fragSource)
    {
        mProgram = CompileProgram(vertSource, fragSource);

        ASSERT_NE(0u, mProgram);
    }

    void drawTest()
    {
        glUseProgram(mProgram);

        GLint position = glGetAttribLocation(mProgram, essl1_shaders::PositionAttrib());
        GLint src0     = glGetUniformLocation(mProgram, "src0");
        GLint src1     = glGetUniformLocation(mProgram, "src1");
        ASSERT_GL_NO_ERROR();

        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glEnableVertexAttribArray(position);
        glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, 0, 0);
        ASSERT_GL_NO_ERROR();

        static const float kDst[4]  = {0.5f, 0.5f, 0.5f, 0.5f};
        static const float kSrc0[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        static const float kSrc1[4] = {0.3f, 0.6f, 0.9f, 0.7f};

        glUniform4f(src0, kSrc0[0], kSrc0[1], kSrc0[2], kSrc0[3]);
        glUniform4f(src1, kSrc1[0], kSrc1[1], kSrc1[2], kSrc1[3]);
        ASSERT_GL_NO_ERROR();

        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glViewport(0, 0, kWidth, kHeight);
        glClearColor(kDst[0], kDst[1], kDst[2], kDst[3]);
        ASSERT_GL_NO_ERROR();

        {
            glBlendFuncSeparate(GL_SRC1_COLOR_EXT, GL_SRC_ALPHA, GL_ONE_MINUS_SRC1_COLOR_EXT,
                                GL_ONE_MINUS_SRC1_ALPHA_EXT);

            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            ASSERT_GL_NO_ERROR();

            // verify
            angle::GLColor color;
            BlendEquationFuncAdd<GL_SRC1_COLOR_EXT, GL_SRC_ALPHA, GL_ONE_MINUS_SRC1_COLOR_EXT,
                                 GL_ONE_MINUS_SRC1_ALPHA_EXT>(kDst, kSrc0, kSrc1, &color);

            CheckPixels(kWidth / 4, (3 * kHeight) / 4, 1, 1, 1, color);
            CheckPixels(kWidth - 1, 0, 1, 1, 1, color);
        }

        {
            glBlendFuncSeparate(GL_ONE_MINUS_SRC1_COLOR_EXT, GL_ONE_MINUS_SRC_ALPHA,
                                GL_ONE_MINUS_SRC_COLOR, GL_SRC1_ALPHA_EXT);

            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            ASSERT_GL_NO_ERROR();

            // verify
            angle::GLColor color;
            BlendEquationFuncAdd<GL_ONE_MINUS_SRC1_COLOR_EXT, GL_ONE_MINUS_SRC_ALPHA,
                                 GL_ONE_MINUS_SRC_COLOR, GL_SRC1_ALPHA_EXT>(kDst, kSrc0, kSrc1,
                                                                            &color);

            CheckPixels(kWidth / 4, (3 * kHeight) / 4, 1, 1, 1, color);
            CheckPixels(kWidth - 1, 0, 1, 1, 1, color);
        }
    }

    GLuint mVBO;
    GLuint mProgram;
};

class EXTBlendFuncExtendedDrawTestES3 : public EXTBlendFuncExtendedDrawTest
{
  protected:
    EXTBlendFuncExtendedDrawTestES3() : EXTBlendFuncExtendedDrawTest(), mIsES31OrNewer(false) {}

    void testSetUp() override
    {
        EXTBlendFuncExtendedDrawTest::testSetUp();
        if (getClientMajorVersion() > 3 ||
            (getClientMajorVersion() == 3 && getClientMinorVersion() >= 1))
        {
            mIsES31OrNewer = true;
        }
    }
    void checkOutputIndexQuery(const char *name, GLint expectedIndex)
    {
        GLint index = glGetFragDataIndexEXT(mProgram, name);
        EXPECT_EQ(expectedIndex, index);
        if (mIsES31OrNewer)
        {
            index = glGetProgramResourceLocationIndexEXT(mProgram, GL_PROGRAM_OUTPUT, name);
            EXPECT_EQ(expectedIndex, index);
        }
        else
        {
            glGetProgramResourceLocationIndexEXT(mProgram, GL_PROGRAM_OUTPUT, name);
            EXPECT_GL_ERROR(GL_INVALID_OPERATION);
        }
    }

    void LinkProgram()
    {
        glLinkProgram(mProgram);
        GLint linked = 0;
        glGetProgramiv(mProgram, GL_LINK_STATUS, &linked);
        EXPECT_NE(0, linked);
        glUseProgram(mProgram);
        return;
    }

  private:
    bool mIsES31OrNewer;
};

}  // namespace

// Test EXT_blend_func_extended related gets.
TEST_P(EXTBlendFuncExtendedTest, TestMaxDualSourceDrawBuffers)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    GLint maxDualSourceDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT, &maxDualSourceDrawBuffers);
    EXPECT_GT(maxDualSourceDrawBuffers, 0);

    ASSERT_GL_NO_ERROR();
}

// Test a shader with EXT_blend_func_extended and gl_SecondaryFragColorEXT.
// Outputs to primary color buffer using primary and secondary colors.
TEST_P(EXTBlendFuncExtendedDrawTest, FragColor)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    const char *kFragColorShader =
        "#extension GL_EXT_blend_func_extended : require\n"
        "precision mediump float;\n"
        "uniform vec4 src0;\n"
        "uniform vec4 src1;\n"
        "void main() {\n"
        "  gl_FragColor = src0;\n"
        "  gl_SecondaryFragColorEXT = src1;\n"
        "}\n";

    makeProgram(essl1_shaders::vs::Simple(), kFragColorShader);

    drawTest();
}

// Test a shader with EXT_blend_func_extended and gl_FragData.
// Outputs to a color buffer using primary and secondary frag data.
TEST_P(EXTBlendFuncExtendedDrawTest, FragData)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    const char *kFragColorShader =
        "#extension GL_EXT_blend_func_extended : require\n"
        "precision mediump float;\n"
        "uniform vec4 src0;\n"
        "uniform vec4 src1;\n"
        "void main() {\n"
        "  gl_FragData[0] = src0;\n"
        "  gl_SecondaryFragDataEXT[0] = src1;\n"
        "}\n";

    makeProgram(essl1_shaders::vs::Simple(), kFragColorShader);

    drawTest();
}

// Test an ESSL 3.00 shader that uses two fragment outputs with locations specified in the shader.
TEST_P(EXTBlendFuncExtendedDrawTestES3, FragmentOutputLocationsInShader)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    const char *kFragColorShader = R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
uniform vec4 src0;
uniform vec4 src1;
layout(location = 0, index = 1) out vec4 outSrc1;
layout(location = 0, index = 0) out vec4 outSrc0;
void main() {
    outSrc0 = src0;
    outSrc1 = src1;
})";

    makeProgram(essl3_shaders::vs::Simple(), kFragColorShader);

    checkOutputIndexQuery("outSrc0", 0);
    checkOutputIndexQuery("outSrc1", 1);

    drawTest();
}

// Test an ESSL 3.00 shader that uses two fragment outputs with locations specified through the API.
TEST_P(EXTBlendFuncExtendedDrawTestES3, FragmentOutputLocationAPI)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    constexpr char kFS[] = R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
uniform vec4 src0;
uniform vec4 src1;
out vec4 outSrc1;
out vec4 outSrc0;
void main() {
    outSrc0 = src0;
    outSrc1 = src1;
})";

    mProgram = CompileProgram(essl3_shaders::vs::Simple(), kFS, [](GLuint program) {
        glBindFragDataLocationIndexedEXT(program, 0, 0, "outSrc0");
        glBindFragDataLocationIndexedEXT(program, 0, 1, "outSrc1");
    });

    ASSERT_NE(0u, mProgram);

    checkOutputIndexQuery("outSrc0", 0);
    checkOutputIndexQuery("outSrc1", 1);

    drawTest();
}

// Test an ESSL 3.00 shader that uses two fragment outputs, with location for one specified through
// the API and location for another being set automatically.
TEST_P(EXTBlendFuncExtendedDrawTestES3, FragmentOutputLocationsAPIAndAutomatic)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    constexpr char kFS[] = R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
uniform vec4 src0;
uniform vec4 src1;
out vec4 outSrc1;
out vec4 outSrc0;
void main() {
    outSrc0 = src0;
    outSrc1 = src1;
})";

    mProgram = CompileProgram(essl3_shaders::vs::Simple(), kFS, [](GLuint program) {
        glBindFragDataLocationIndexedEXT(program, 0, 1, "outSrc1");
    });

    ASSERT_NE(0u, mProgram);

    checkOutputIndexQuery("outSrc0", 0);
    checkOutputIndexQuery("outSrc1", 1);

    drawTest();
}

// Test an ESSL 3.00 shader that uses two array fragment outputs with locations specified through
// the API.
TEST_P(EXTBlendFuncExtendedDrawTestES3, FragmentArrayOutputLocationsAPI)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    // TODO: Investigate this mac-only failure.  http://angleproject.com/1085
    ANGLE_SKIP_TEST_IF(IsOSX());

    constexpr char kFS[] = R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
uniform vec4 src0;
uniform vec4 src1;
out vec4 outSrc1[1];
out vec4 outSrc0[1];
void main() {
    outSrc0[0] = src0;
    outSrc1[0] = src1;
})";

    mProgram = CompileProgram(essl3_shaders::vs::Simple(), kFS, [](GLuint program) {
        // Specs aren't very clear on what kind of name should be used when binding location for
        // array variables. We only allow names that do include the "[0]" suffix.
        glBindFragDataLocationIndexedEXT(program, 0, 0, "outSrc0[0]");
        glBindFragDataLocationIndexedEXT(program, 0, 1, "outSrc1[0]");
    });

    ASSERT_NE(0u, mProgram);

    // The extension spec is not very clear on what name can be used for the queries for array
    // variables. We're checking that the queries work in the same way as specified in OpenGL 4.4
    // page 107.
    checkOutputIndexQuery("outSrc0[0]", 0);
    checkOutputIndexQuery("outSrc1[0]", 1);
    checkOutputIndexQuery("outSrc0", 0);
    checkOutputIndexQuery("outSrc1", 1);

    // These queries use an out of range array index so they should return -1.
    checkOutputIndexQuery("outSrc0[1]", -1);
    checkOutputIndexQuery("outSrc1[1]", -1);

    drawTest();
}

// Ported from TranslatorVariants/EXTBlendFuncExtendedES3DrawTest
// Test that tests glBindFragDataLocationEXT, glBindFragDataLocationIndexedEXT,
// glGetFragDataLocation, glGetFragDataIndexEXT work correctly with
// GLSL array output variables. The output variable can be bound by
// referring to the variable name with or without the first element array
// accessor. The getters can query location of the individual elements in
// the array. The test does not actually use the base test drawing,
// since the drivers at the time of writing do not support multiple
// buffers and dual source blending.
TEST_P(EXTBlendFuncExtendedDrawTestES3, ES3GettersArray)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    // TODO(zmo): Figure out why this fails on AMD. crbug.com/585132.
    // Also fails on the Intel Mesa driver, see
    // https://bugs.freedesktop.org/show_bug.cgi?id=96765
    ANGLE_SKIP_TEST_IF(IsLinux() && IsAMD());
    ANGLE_SKIP_TEST_IF(IsLinux() && IsIntel());

    const GLint kTestArraySize     = 2;
    const GLint kFragData0Location = 2;
    const GLint kFragData1Location = 1;
    const GLint kUnusedLocation    = 5;

    // The test binds kTestArraySize -sized array to location 1 for test purposes.
    // The GL_MAX_DRAW_BUFFERS must be > kTestArraySize, since an
    // array will be bound to continuous locations, starting from the first
    // location.
    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS_EXT, &maxDrawBuffers);
    EXPECT_LT(kTestArraySize, maxDrawBuffers);

    constexpr char kFragColorShader[] = R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
uniform vec4 src;
uniform vec4 src1;
out vec4 FragData[2];
void main() {
    FragData[0] = src;
    FragData[1] = src1;
})";

    struct testCase
    {
        std::string unusedLocationName;
        std::string fragData0LocationName;
        std::string fragData1LocationName;
    };

    testCase testCases[4]{{"FragData[0]", "FragData", "FragData[1]"},
                          {"FragData", "FragData[0]", "FragData[1]"},
                          {"FragData[0]", "FragData", "FragData[1]"},
                          {"FragData", "FragData[0]", "FragData[1]"}};

    for (const testCase &test : testCases)
    {
        mProgram =
            CompileProgram(essl3_shaders::vs::Simple(), kFragColorShader, [&](GLuint program) {
                glBindFragDataLocationEXT(program, kUnusedLocation,
                                          test.unusedLocationName.c_str());
                glBindFragDataLocationEXT(program, kFragData0Location,
                                          test.fragData0LocationName.c_str());
                glBindFragDataLocationEXT(program, kFragData1Location,
                                          test.fragData1LocationName.c_str());
            });

        EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
        LinkProgram();
        EXPECT_EQ(kFragData0Location, glGetFragDataLocation(mProgram, "FragData"));
        EXPECT_EQ(0, glGetFragDataIndexEXT(mProgram, "FragData"));
        EXPECT_EQ(kFragData0Location, glGetFragDataLocation(mProgram, "FragData[0]"));
        EXPECT_EQ(0, glGetFragDataIndexEXT(mProgram, "FragData[0]"));
        EXPECT_EQ(kFragData1Location, glGetFragDataLocation(mProgram, "FragData[1]"));
        EXPECT_EQ(0, glGetFragDataIndexEXT(mProgram, "FragData[1]"));
        // Index bigger than the GLSL variable array length does not find anything.
        EXPECT_EQ(-1, glGetFragDataLocation(mProgram, "FragData[3]"));
    }
}

// Ported from TranslatorVariants/EXTBlendFuncExtendedES3DrawTest
TEST_P(EXTBlendFuncExtendedDrawTestES3, ESSL3BindSimpleVarAsArrayNoBind)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    constexpr char kFragDataShader[] = R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
uniform vec4 src;
uniform vec4 src1;
out vec4 FragData;
out vec4 SecondaryFragData;
void main() {
    FragData = src;
    SecondaryFragData = src1;
})";

    mProgram = CompileProgram(essl3_shaders::vs::Simple(), kFragDataShader, [](GLuint program) {
        glBindFragDataLocationEXT(program, 0, "FragData[0]");
        glBindFragDataLocationIndexedEXT(program, 0, 1, "SecondaryFragData[0]");
    });

    LinkProgram();

    EXPECT_EQ(-1, glGetFragDataLocation(mProgram, "FragData[0]"));
    EXPECT_EQ(0, glGetFragDataLocation(mProgram, "FragData"));
    EXPECT_EQ(1, glGetFragDataLocation(mProgram, "SecondaryFragData"));
    // Did not bind index.
    EXPECT_EQ(0, glGetFragDataIndexEXT(mProgram, "SecondaryFragData"));

    glBindFragDataLocationEXT(mProgram, 0, "FragData");
    glBindFragDataLocationIndexedEXT(mProgram, 0, 1, "SecondaryFragData");
    LinkProgram();
}

// Test an ESSL 3.00 program with a link-time fragment output location conflict.
TEST_P(EXTBlendFuncExtendedTestES3, FragmentOutputLocationConflict)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    constexpr char kFS[] = R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
uniform vec4 src0;
uniform vec4 src1;
out vec4 out0;
out vec4 out1;
void main() {
    out0 = src0;
    out1 = src1;
})";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, essl3_shaders::vs::Simple());
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFS);
    ASSERT_NE(0u, vs);
    ASSERT_NE(0u, fs);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glDeleteShader(vs);
    glAttachShader(program, fs);
    glDeleteShader(fs);

    glBindFragDataLocationIndexedEXT(program, 0, 0, "out0");
    glBindFragDataLocationIndexedEXT(program, 0, 0, "out1");

    // The program should fail to link.
    glLinkProgram(program);
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_EQ(0, linkStatus);

    glDeleteProgram(program);
}

// Test an ESSL 3.00 program with some bindings set for nonexistent variables. These should not
// create link-time conflicts.
TEST_P(EXTBlendFuncExtendedTestES3, FragmentOutputLocationForNonexistentOutput)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    constexpr char kFS[] = R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
uniform vec4 src0;
out vec4 out0;
void main() {
    out0 = src0;
})";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, essl3_shaders::vs::Simple());
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFS);
    ASSERT_NE(0u, vs);
    ASSERT_NE(0u, fs);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glDeleteShader(vs);
    glAttachShader(program, fs);
    glDeleteShader(fs);

    glBindFragDataLocationIndexedEXT(program, 0, 0, "out0");
    glBindFragDataLocationIndexedEXT(program, 0, 0, "out1");
    glBindFragDataLocationIndexedEXT(program, 0, 0, "out2[0]");

    // The program should link successfully - conflicting location for nonexistent variables out1 or
    // out2 should not be an issue.
    glLinkProgram(program);
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_NE(0, linkStatus);

    glDeleteProgram(program);
}

// Test mixing shader-assigned and automatic output locations.
TEST_P(EXTBlendFuncExtendedTestES3, FragmentOutputLocationsPartiallyAutomatic)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    GLint maxDrawBuffers;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    ANGLE_SKIP_TEST_IF(maxDrawBuffers < 4);

    constexpr char kFS[] = R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
uniform vec4 src0;
uniform vec4 src1;
uniform vec4 src2;
uniform vec4 src3;
layout(location=0) out vec4 out0;
layout(location=3) out vec4 out3;
out vec4 out12[2];
void main() {
    out0 = src0;
    out12[0] = src1;
    out12[1] = src2;
    out3 = src3;
})";

    GLuint program = CompileProgram(essl3_shaders::vs::Simple(), kFS);
    ASSERT_NE(0u, program);

    GLint location = glGetFragDataLocation(program, "out0");
    EXPECT_EQ(0, location);
    location = glGetFragDataLocation(program, "out12");
    EXPECT_EQ(1, location);
    location = glGetFragDataLocation(program, "out3");
    EXPECT_EQ(3, location);

    glDeleteProgram(program);
}

// Test a fragment output array that doesn't fit because contiguous locations are not available.
TEST_P(EXTBlendFuncExtendedTestES3, FragmentOutputArrayDoesntFit)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_blend_func_extended"));

    GLint maxDrawBuffers;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    ANGLE_SKIP_TEST_IF(maxDrawBuffers < 4);

    std::stringstream fragShader;
    fragShader << R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
layout(location=2) out vec4 out0;
out vec4 outArray[)"
               << (maxDrawBuffers - 1) << R"(];
void main() {
    out0 = vec4(1.0);
    outArray[0] = vec4(1.0);
})";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, essl3_shaders::vs::Simple());
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragShader.str().c_str());
    ASSERT_NE(0u, vs);
    ASSERT_NE(0u, fs);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glDeleteShader(vs);
    glAttachShader(program, fs);
    glDeleteShader(fs);

    // The program should not link - there's no way to fit "outArray" into available output
    // locations.
    glLinkProgram(program);
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_EQ(0, linkStatus);

    glDeleteProgram(program);
}

ANGLE_INSTANTIATE_TEST_ES2(EXTBlendFuncExtendedTest);
ANGLE_INSTANTIATE_TEST_ES3_AND_ES31(EXTBlendFuncExtendedTestES3);

ANGLE_INSTANTIATE_TEST_ES2(EXTBlendFuncExtendedDrawTest);
ANGLE_INSTANTIATE_TEST_ES3_AND_ES31(EXTBlendFuncExtendedDrawTestES3);
