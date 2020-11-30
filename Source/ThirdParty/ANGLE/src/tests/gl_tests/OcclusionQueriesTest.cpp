//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"
#include "util/EGLWindow.h"
#include "util/random_utils.h"
#include "util/test_utils.h"

using namespace angle;

class OcclusionQueriesTest : public ANGLETest
{
  protected:
    OcclusionQueriesTest() : mProgram(0), mRNG(1)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
    }

    void testSetUp() override
    {
        mProgram = CompileProgram(essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
        ASSERT_NE(0u, mProgram);
    }

    void testTearDown() override { glDeleteProgram(mProgram); }

    GLuint mProgram;
    RNG mRNG;
};

TEST_P(OcclusionQueriesTest, IsOccluded)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       !IsGLExtensionEnabled("GL_EXT_occlusion_query_boolean"));

    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // draw a quad at depth 0.3
    glEnable(GL_DEPTH_TEST);
    glUseProgram(mProgram);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.3f);
    glUseProgram(0);

    EXPECT_GL_NO_ERROR();

    GLuint query = 0;
    glGenQueriesEXT(1, &query);
    glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, query);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(),
             0.8f);  // this quad should be occluded by first quad
    glEndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);

    EXPECT_GL_NO_ERROR();

    swapBuffers();

    GLuint ready = GL_FALSE;
    while (ready == GL_FALSE)
    {
        angle::Sleep(0);
        glGetQueryObjectuivEXT(query, GL_QUERY_RESULT_AVAILABLE_EXT, &ready);
    }

    GLuint result = GL_TRUE;
    glGetQueryObjectuivEXT(query, GL_QUERY_RESULT_EXT, &result);

    EXPECT_GL_NO_ERROR();

    glDeleteQueriesEXT(1, &query);

    EXPECT_GL_FALSE(result);
}

TEST_P(OcclusionQueriesTest, IsNotOccluded)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       !IsGLExtensionEnabled("GL_EXT_occlusion_query_boolean"));

    // TODO(syoussefi): Using render pass ops to clear the framebuffer attachment results in
    // AMD/Windows misbehaving in this test.  http://anglebug.com/3286
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && IsVulkan());

    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    EXPECT_GL_NO_ERROR();

    GLuint query = 0;
    glGenQueriesEXT(1, &query);
    glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, query);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.8f);  // this quad should not be occluded
    glEndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);

    EXPECT_GL_NO_ERROR();

    swapBuffers();

    GLuint result = GL_TRUE;
    glGetQueryObjectuivEXT(query, GL_QUERY_RESULT_EXT, &result);  // will block waiting for result

    EXPECT_GL_NO_ERROR();

    glDeleteQueriesEXT(1, &query);

    EXPECT_GL_TRUE(result);
}

TEST_P(OcclusionQueriesTest, Errors)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       !IsGLExtensionEnabled("GL_EXT_occlusion_query_boolean"));

    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    EXPECT_GL_NO_ERROR();

    GLuint query  = 0;
    GLuint query2 = 0;
    glGenQueriesEXT(1, &query);

    EXPECT_GL_FALSE(glIsQueryEXT(query));
    EXPECT_GL_FALSE(glIsQueryEXT(query2));

    glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, 0);  // can't pass 0 as query id
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, query);
    glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT,
                    query2);  // can't initiate a query while one's already active
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    EXPECT_GL_TRUE(glIsQueryEXT(query));
    EXPECT_GL_FALSE(glIsQueryEXT(query2));  // have not called begin

    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.8f);  // this quad should not be occluded
    glEndQueryEXT(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT);      // no active query for this target
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glEndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);

    glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT,
                    query);  // can't begin a query as a different type than previously used
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT,
                    query2);  // have to call genqueries first
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glGenQueriesEXT(1, &query2);
    glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT, query2);  // should be ok now
    EXPECT_GL_TRUE(glIsQueryEXT(query2));

    drawQuad(mProgram, essl1_shaders::PositionAttrib(),
             0.3f);                  // this should draw in front of other quad
    glDeleteQueriesEXT(1, &query2);  // should delete when query becomes inactive
    glEndQueryEXT(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT);  // should not incur error; should delete
                                                            // query + 1 at end of execution.
    EXPECT_GL_NO_ERROR();

    swapBuffers();

    EXPECT_GL_NO_ERROR();

    GLuint ready = GL_FALSE;
    glGetQueryObjectuivEXT(query2, GL_QUERY_RESULT_AVAILABLE_EXT,
                           &ready);  // this query is now deleted
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    EXPECT_GL_NO_ERROR();
}

// Test that running multiple simultaneous queries from multiple EGL contexts returns the correct
// result for each query.  Helps expose bugs in ANGLE's virtual contexts.
TEST_P(OcclusionQueriesTest, MultiContext)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       !IsGLExtensionEnabled("GL_EXT_occlusion_query_boolean"));

    // TODO(cwallez@chromium.org): Suppression for http://anglebug.com/3080
    ANGLE_SKIP_TEST_IF(IsWindows() && IsNVIDIA() && IsVulkan());

    // Test skipped because the D3D backends cannot support simultaneous queries on multiple
    // contexts yet.  Same with the Vulkan backend.
    ANGLE_SKIP_TEST_IF(GetParam() == ES2_D3D9() || GetParam() == ES2_D3D11() ||
                       GetParam() == ES3_D3D11() || GetParam() == ES2_VULKAN());

    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(IsVulkan());

    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // draw a quad at depth 0.5
    glEnable(GL_DEPTH_TEST);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);

    EGLWindow *window = getEGLWindow();

    EGLDisplay display = window->getDisplay();
    EGLConfig config   = window->getConfig();
    EGLSurface surface = window->getSurface();

    EGLint contextAttributes[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR,
        GetParam().majorVersion,
        EGL_CONTEXT_MINOR_VERSION_KHR,
        GetParam().minorVersion,
        EGL_NONE,
    };

    const size_t passCount = 5;
    struct ContextInfo
    {
        EGLContext context;
        GLuint program;
        GLuint query;
        bool visiblePasses[passCount];
        bool shouldPass;
    };

    ContextInfo contexts[] = {
        {
            EGL_NO_CONTEXT,
            0,
            0,
            {false, false, false, false, false},
            false,
        },
        {
            EGL_NO_CONTEXT,
            0,
            0,
            {false, true, false, true, false},
            true,
        },
        {
            EGL_NO_CONTEXT,
            0,
            0,
            {false, false, false, false, false},
            false,
        },
        {
            EGL_NO_CONTEXT,
            0,
            0,
            {true, true, false, true, true},
            true,
        },
        {
            EGL_NO_CONTEXT,
            0,
            0,
            {false, true, true, true, true},
            true,
        },
        {
            EGL_NO_CONTEXT,
            0,
            0,
            {true, false, false, true, false},
            true,
        },
        {
            EGL_NO_CONTEXT,
            0,
            0,
            {false, false, false, false, false},
            false,
        },
        {
            EGL_NO_CONTEXT,
            0,
            0,
            {false, false, false, false, false},
            false,
        },
    };

    for (auto &context : contexts)
    {
        context.context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
        ASSERT_NE(context.context, EGL_NO_CONTEXT);

        eglMakeCurrent(display, surface, surface, context.context);

        context.program = CompileProgram(essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
        ASSERT_NE(context.program, 0u);

        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);

        glGenQueriesEXT(1, &context.query);
        glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, context.query);

        ASSERT_GL_NO_ERROR();
    }

    for (size_t pass = 0; pass < passCount; pass++)
    {
        for (const auto &context : contexts)
        {
            eglMakeCurrent(display, surface, surface, context.context);

            float depth = context.visiblePasses[pass] ? mRNG.randomFloatBetween(0.0f, 0.4f)
                                                      : mRNG.randomFloatBetween(0.6f, 1.0f);
            drawQuad(context.program, essl1_shaders::PositionAttrib(), depth);

            EXPECT_GL_NO_ERROR();
        }
    }

    for (const auto &context : contexts)
    {
        eglMakeCurrent(display, surface, surface, context.context);
        glEndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);

        GLuint result = GL_TRUE;
        glGetQueryObjectuivEXT(context.query, GL_QUERY_RESULT_EXT, &result);

        EXPECT_GL_NO_ERROR();

        GLuint expectation = context.shouldPass ? GL_TRUE : GL_FALSE;
        EXPECT_EQ(expectation, result);
    }

    eglMakeCurrent(display, surface, surface, window->getContext());

    for (auto &context : contexts)
    {
        eglDestroyContext(display, context.context);
        context.context = EGL_NO_CONTEXT;
    }
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(OcclusionQueriesTest);
