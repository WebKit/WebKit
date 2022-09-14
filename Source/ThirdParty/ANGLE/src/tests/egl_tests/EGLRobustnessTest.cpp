//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// EGLRobustnessTest.cpp: tests for EGL_EXT_create_context_robustness
//
// Tests causing GPU resets are disabled, use the following args to run them:
// --gtest_also_run_disabled_tests --gtest_filter=EGLRobustnessTest\*

#include <gtest/gtest.h>

#include "common/debug.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "util/OSWindow.h"

using namespace angle;

class EGLRobustnessTest : public ANGLETest<>
{
  public:
    void testSetUp() override
    {
        mOSWindow = OSWindow::New();
        mOSWindow->initialize("EGLRobustnessTest", 500, 500);
        setWindowVisible(mOSWindow, true);

        const auto &platform = GetParam().eglParameters;

        std::vector<EGLint> displayAttributes;
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
        displayAttributes.push_back(platform.renderer);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE);
        displayAttributes.push_back(platform.majorVersion);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE);
        displayAttributes.push_back(platform.minorVersion);

        if (platform.deviceType != EGL_DONT_CARE)
        {
            displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
            displayAttributes.push_back(platform.deviceType);
        }

        displayAttributes.push_back(EGL_NONE);

        mDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE,
                                            reinterpret_cast<void *>(mOSWindow->getNativeDisplay()),
                                            &displayAttributes[0]);
        ASSERT_NE(EGL_NO_DISPLAY, mDisplay);

        ASSERT_TRUE(eglInitialize(mDisplay, nullptr, nullptr) == EGL_TRUE);

        const char *extensions = eglQueryString(mDisplay, EGL_EXTENSIONS);
        if (strstr(extensions, "EGL_EXT_create_context_robustness") == nullptr)
        {
            std::cout << "Test skipped due to missing EGL_EXT_create_context_robustness"
                      << std::endl;
            return;
        }

        int nConfigs = 0;
        ASSERT_TRUE(eglGetConfigs(mDisplay, nullptr, 0, &nConfigs) == EGL_TRUE);
        ASSERT_LE(1, nConfigs);

        std::vector<EGLConfig> allConfigs(nConfigs);
        int nReturnedConfigs = 0;
        ASSERT_TRUE(eglGetConfigs(mDisplay, allConfigs.data(), nConfigs, &nReturnedConfigs) ==
                    EGL_TRUE);
        ASSERT_EQ(nConfigs, nReturnedConfigs);

        for (const EGLConfig &config : allConfigs)
        {
            EGLint surfaceType;
            eglGetConfigAttrib(mDisplay, config, EGL_SURFACE_TYPE, &surfaceType);

            if ((surfaceType & EGL_WINDOW_BIT) != 0)
            {
                mConfig      = config;
                mInitialized = true;
                break;
            }
        }

        if (mInitialized)
        {
            mWindow =
                eglCreateWindowSurface(mDisplay, mConfig, mOSWindow->getNativeWindow(), nullptr);
            ASSERT_EGL_SUCCESS();
        }
    }

    void testTearDown() override
    {
        eglDestroySurface(mDisplay, mWindow);
        eglDestroyContext(mDisplay, mContext);
        eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglTerminate(mDisplay);
        EXPECT_EGL_SUCCESS();

        OSWindow::Delete(&mOSWindow);
    }

    void createContext(EGLint resetStrategy)
    {
        const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
                                         EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT,
                                         resetStrategy, EGL_NONE};
        mContext = eglCreateContext(mDisplay, mConfig, EGL_NO_CONTEXT, contextAttribs);
        ASSERT_NE(EGL_NO_CONTEXT, mContext);

        eglMakeCurrent(mDisplay, mWindow, mWindow, mContext);
        ASSERT_EGL_SUCCESS();

        const char *extensionString = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
        ASSERT_NE(nullptr, strstr(extensionString, "GL_ANGLE_instanced_arrays"));
    }

    void createRobustContext(EGLint resetStrategy, EGLContext shareContext)
    {
        const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION,
                                         3,
                                         EGL_CONTEXT_MINOR_VERSION_KHR,
                                         0,
                                         EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT,
                                         EGL_TRUE,
                                         EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT,
                                         resetStrategy,
                                         EGL_NONE};

        mContext = eglCreateContext(mDisplay, mConfig, shareContext, contextAttribs);
        ASSERT_NE(EGL_NO_CONTEXT, mContext);

        eglMakeCurrent(mDisplay, mWindow, mWindow, mContext);
        ASSERT_EGL_SUCCESS();
    }

    void forceContextReset()
    {
        // Cause a GPU reset by drawing 100,000,000 fullscreen quads
        GLuint program = CompileProgram(
            "attribute vec4 pos;\n"
            "void main() {gl_Position = pos;}\n",
            "precision mediump float;\n"
            "void main() {gl_FragColor = vec4(1.0);}\n");
        ASSERT_NE(0u, program);
        glUseProgram(program);

        GLfloat vertices[] = {
            -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f,
            1.0f,  -1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 0.0f, 1.0f,
        };

        const int kNumQuads = 10000;
        std::vector<GLushort> indices(6 * kNumQuads);

        for (size_t i = 0; i < kNumQuads; i++)
        {
            indices[i * 6 + 0] = 0;
            indices[i * 6 + 1] = 1;
            indices[i * 6 + 2] = 2;
            indices[i * 6 + 3] = 1;
            indices[i * 6 + 4] = 2;
            indices[i * 6 + 5] = 3;
        }

        glBindAttribLocation(program, 0, "pos");
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, vertices);
        glEnableVertexAttribArray(0);

        glViewport(0, 0, mOSWindow->getWidth(), mOSWindow->getHeight());
        glClearColor(1.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawElementsInstancedANGLE(GL_TRIANGLES, kNumQuads * 6, GL_UNSIGNED_SHORT, indices.data(),
                                     10000);

        glFinish();
    }

    const char *getInvalidShaderLocalVariableAccessFS()
    {
        static constexpr char kFS[] = R"(#version 300 es
            layout(location = 0) out highp vec4 fragColor;
            uniform highp int u_index;
            uniform mediump float u_color;

            void main (void)
            {
                highp vec4 color = vec4(0.0f);
                color[u_index] = u_color;
                fragColor = color;
            })";

        return kFS;
    }

    void testInvalidShaderLocalVariableAccess(GLuint program)
    {
        glUseProgram(program);
        EXPECT_GL_NO_ERROR();

        GLint indexLocation = glGetUniformLocation(program, "u_index");
        ASSERT_NE(-1, indexLocation);
        GLint colorLocation = glGetUniformLocation(program, "u_color");
        ASSERT_NE(-1, colorLocation);

        // delibrately pass in -1 to u_index to test robustness extension protects write out of
        // bound
        constexpr GLint kInvalidIndex = -1;
        glUniform1i(indexLocation, kInvalidIndex);
        EXPECT_GL_NO_ERROR();

        glUniform1f(colorLocation, 1.0f);

        drawQuad(program, essl31_shaders::PositionAttrib(), 0);
        EXPECT_GL_NO_ERROR();

        // When command buffers are submitted to GPU, if robustness is working properly, the
        // fragment shader will not suffer from write out-of-bounds issue, which resulted in context
        // reset and context loss.
        glFinish();
        EXPECT_GL_NO_ERROR();
    }

  protected:
    EGLDisplay mDisplay = EGL_NO_DISPLAY;
    EGLSurface mWindow  = EGL_NO_SURFACE;
    EGLContext mContext = EGL_NO_CONTEXT;
    bool mInitialized   = false;

  private:
    EGLConfig mConfig   = 0;
    OSWindow *mOSWindow = nullptr;
};

class EGLRobustnessTestES3 : public EGLRobustnessTest
{};

// Check glGetGraphicsResetStatusEXT returns GL_NO_ERROR if we did nothing
TEST_P(EGLRobustnessTest, NoErrorByDefault)
{
    ANGLE_SKIP_TEST_IF(!mInitialized);
    ASSERT_TRUE(glGetGraphicsResetStatusEXT() == GL_NO_ERROR);
}

// Checks that the application gets no loss with NO_RESET_NOTIFICATION
TEST_P(EGLRobustnessTest, DISABLED_NoResetNotification)
{
    ANGLE_SKIP_TEST_IF(!mInitialized);
    createContext(EGL_NO_RESET_NOTIFICATION_EXT);

    if (!IsWindows())
    {
        std::cout << "Test disabled on non Windows platforms because drivers can't recover. "
                  << "See " << __FILE__ << ":" << __LINE__ << std::endl;
        return;
    }
    std::cout << "Causing a GPU reset, brace for impact." << std::endl;

    forceContextReset();
    ASSERT_TRUE(glGetGraphicsResetStatusEXT() == GL_NO_ERROR);
}

// Checks that resetting the ANGLE display allows to get rid of the context loss.
// Also checks that the application gets notified of the loss of the display.
// We coalesce both tests to reduce the number of TDRs done on Windows: by default
// having more than 5 TDRs in a minute will cause Windows to disable the GPU until
// the computer is rebooted.
TEST_P(EGLRobustnessTest, DISABLED_ResettingDisplayWorks)
{
    // Note that on Windows the OpenGL driver fails hard (popup that closes the application)
    // on a TDR caused by D3D. Don't run D3D tests at the same time as the OpenGL tests.
    ANGLE_SKIP_TEST_IF(IsWindows() && isGLRenderer());
    ANGLE_SKIP_TEST_IF(!mInitialized);

    createContext(EGL_LOSE_CONTEXT_ON_RESET_EXT);

    if (!IsWindows())
    {
        std::cout << "Test disabled on non Windows platforms because drivers can't recover. "
                  << "See " << __FILE__ << ":" << __LINE__ << std::endl;
        return;
    }
    std::cout << "Causing a GPU reset, brace for impact." << std::endl;

    forceContextReset();
    ASSERT_TRUE(glGetGraphicsResetStatusEXT() != GL_NO_ERROR);

    recreateTestFixture();
    ASSERT_TRUE(glGetGraphicsResetStatusEXT() == GL_NO_ERROR);
}

// Test to reproduce the crash when running
// dEQP-EGL.functional.robustness.reset_context.shaders.out_of_bounds.reset_status.writes.uniform_block.fragment
// on Pixel 6
TEST_P(EGLRobustnessTestES3, ContextResetOnInvalidLocalShaderVariableAccess)
{
    ANGLE_SKIP_TEST_IF(!mInitialized);

    ANGLE_SKIP_TEST_IF(
        !IsEGLDisplayExtensionEnabled(mDisplay, "EGL_KHR_create_context") ||
        !IsEGLDisplayExtensionEnabled(mDisplay, "EGL_EXT_create_context_robustness"));

    createRobustContext(EGL_LOSE_CONTEXT_ON_RESET, EGL_NO_CONTEXT);

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), getInvalidShaderLocalVariableAccessFS());
    testInvalidShaderLocalVariableAccess(program);
}

// Similar to ContextResetOnInvalidLocalShaderVariableAccess, but the program is created on a
// context that's not robust, but used on one that is.
TEST_P(EGLRobustnessTestES3,
       ContextResetOnInvalidLocalShaderVariableAccess_ShareGroupBeforeProgramCreation)
{
    ANGLE_SKIP_TEST_IF(!mInitialized);

    ANGLE_SKIP_TEST_IF(
        !IsEGLDisplayExtensionEnabled(mDisplay, "EGL_KHR_create_context") ||
        !IsEGLDisplayExtensionEnabled(mDisplay, "EGL_EXT_create_context_robustness"));

    createContext(EGL_LOSE_CONTEXT_ON_RESET);
    EGLContext shareContext = mContext;
    createRobustContext(EGL_LOSE_CONTEXT_ON_RESET, shareContext);

    eglMakeCurrent(mDisplay, mWindow, mWindow, shareContext);
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), getInvalidShaderLocalVariableAccessFS());
    eglMakeCurrent(mDisplay, mWindow, mWindow, mContext);

    testInvalidShaderLocalVariableAccess(program);

    eglDestroyContext(mDisplay, shareContext);
}

// Similar to ContextResetOnInvalidLocalShaderVariableAccess, but the program is created on a
// context that's not robust, but used on one that is.
TEST_P(EGLRobustnessTestES3,
       ContextResetOnInvalidLocalShaderVariableAccess_ShareGroupAfterProgramCreation)
{
    ANGLE_SKIP_TEST_IF(!mInitialized);

    ANGLE_SKIP_TEST_IF(
        !IsEGLDisplayExtensionEnabled(mDisplay, "EGL_KHR_create_context") ||
        !IsEGLDisplayExtensionEnabled(mDisplay, "EGL_EXT_create_context_robustness"));

    createContext(EGL_LOSE_CONTEXT_ON_RESET);
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), getInvalidShaderLocalVariableAccessFS());

    EGLContext shareContext = mContext;

    createRobustContext(EGL_LOSE_CONTEXT_ON_RESET, shareContext);
    testInvalidShaderLocalVariableAccess(program);

    eglDestroyContext(mDisplay, shareContext);
}

// Test to ensure shader local variable write out of bound won't crash
// when the context has robustness enabled, and EGL_NO_RESET_NOTIFICATION_EXT
// is set as the value for attribute EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEFY_EXT
TEST_P(EGLRobustnessTestES3, ContextNoResetOnInvalidLocalShaderVariableAccess)
{
    ANGLE_SKIP_TEST_IF(!mInitialized);
    ANGLE_SKIP_TEST_IF(
        !IsEGLDisplayExtensionEnabled(mDisplay, "EGL_KHR_create_context") ||
        !IsEGLDisplayExtensionEnabled(mDisplay, "EGL_EXT_create_context_robustness"));

    createRobustContext(EGL_NO_RESET_NOTIFICATION_EXT, EGL_NO_CONTEXT);

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), getInvalidShaderLocalVariableAccessFS());
    testInvalidShaderLocalVariableAccess(program);
}

// Similar to ContextNoResetOnInvalidLocalShaderVariableAccess, but the program is created on a
// context that's not robust, but used on one that is.
TEST_P(EGLRobustnessTestES3,
       ContextNoResetOnInvalidLocalShaderVariableAccess_ShareGroupBeforeProgramCreation)
{
    ANGLE_SKIP_TEST_IF(!mInitialized);
    ANGLE_SKIP_TEST_IF(
        !IsEGLDisplayExtensionEnabled(mDisplay, "EGL_KHR_create_context") ||
        !IsEGLDisplayExtensionEnabled(mDisplay, "EGL_EXT_create_context_robustness"));

    createContext(EGL_NO_RESET_NOTIFICATION_EXT);
    EGLContext shareContext = mContext;
    createRobustContext(EGL_NO_RESET_NOTIFICATION_EXT, shareContext);

    eglMakeCurrent(mDisplay, mWindow, mWindow, shareContext);
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), getInvalidShaderLocalVariableAccessFS());
    eglMakeCurrent(mDisplay, mWindow, mWindow, mContext);

    testInvalidShaderLocalVariableAccess(program);

    eglDestroyContext(mDisplay, shareContext);
}

// Similar to ContextNoResetOnInvalidLocalShaderVariableAccess, but the program is created on a
// context that's not robust, but used on one that is.
TEST_P(EGLRobustnessTestES3,
       ContextNoResetOnInvalidLocalShaderVariableAccess_ShareGroupAfterProgramCreation)
{
    ANGLE_SKIP_TEST_IF(!mInitialized);
    ANGLE_SKIP_TEST_IF(
        !IsEGLDisplayExtensionEnabled(mDisplay, "EGL_KHR_create_context") ||
        !IsEGLDisplayExtensionEnabled(mDisplay, "EGL_EXT_create_context_robustness"));

    createContext(EGL_NO_RESET_NOTIFICATION_EXT);
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), getInvalidShaderLocalVariableAccessFS());

    EGLContext shareContext = mContext;

    createRobustContext(EGL_NO_RESET_NOTIFICATION_EXT, shareContext);
    testInvalidShaderLocalVariableAccess(program);

    eglDestroyContext(mDisplay, shareContext);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLRobustnessTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLRobustnessTestES3);
ANGLE_INSTANTIATE_TEST(EGLRobustnessTest,
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES2_D3D9()),
                       WithNoFixture(ES2_D3D11()),
                       WithNoFixture(ES2_OPENGL()),
                       WithNoFixture(ES2_OPENGLES()));
ANGLE_INSTANTIATE_TEST(EGLRobustnessTestES3,
                       WithNoFixture(ES3_VULKAN()),
                       WithNoFixture(ES3_D3D11()),
                       WithNoFixture(ES3_OPENGL()),
                       WithNoFixture(ES3_OPENGLES()));
