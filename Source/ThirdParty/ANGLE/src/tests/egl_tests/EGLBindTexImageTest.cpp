//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLBindTexImageTest.cpp:
//   Tests for eglBindTexImage
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include <gtest/gtest.h>

#include <iostream>
#include "test_utils/ANGLETest.h"
#include "test_utils/angle_test_configs.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"
#include "util/OSWindow.h"
#include "util/test_utils.h"

using namespace angle;

enum class FlushMode
{
    GlSyncImplicitSubmitEnabled,
    ExplicitGlFlushWithGlSyncImplicitSubmitDisabled,
    GlClientWaitFlagFlushWithGlSyncImplicitSubmitDisabled,
    GlSyncImplicitSubmitDisabled
};

enum class ContextMode
{
    SingleContext,
    MultiContext,
};

using EGLBindTexImageTestParams = std::tuple<PlatformParameters, FlushMode, ContextMode>;

std::string PrintBindTexImageTestParamsToString(
    const ::testing::TestParamInfo<EGLBindTexImageTestParams> &info)
{
    PlatformParameters platformParams = std::get<0>(info.param);
    FlushMode flushMode               = std::get<1>(info.param);
    ContextMode contextMode           = std::get<2>(info.param);
    std::stringstream ss;
    ss << platformParams << "_";
    switch (flushMode)
    {
        case FlushMode::GlSyncImplicitSubmitEnabled:
            ss << "_Implicit_Submit";
            break;
        case FlushMode::ExplicitGlFlushWithGlSyncImplicitSubmitDisabled:
            ss << "_Explicit_Flush_No_Implicit_Submit";
            break;
        case FlushMode::GlClientWaitFlagFlushWithGlSyncImplicitSubmitDisabled:
            ss << "_GlClientWait_Flag_No_Implicit_Submit";
            break;
        case FlushMode::GlSyncImplicitSubmitDisabled:
            ss << "_No_Implicit_Submit";
            break;
    }
    switch (contextMode)
    {
        case ContextMode::SingleContext:
            ss << "_Single_Context";
            break;
        case ContextMode::MultiContext:
            ss << "_Multiple_Context";
            break;
    }
    return ss.str();
}

class EGLBindTexImageTest : public ANGLETest<EGLBindTexImageTestParams>
{
  public:
    EGLBindTexImageTest() : mDisplay(EGL_NO_DISPLAY) {}

    void testSetUp() override
    {
        PlatformParameters platformParams = std::get<0>(GetParam());
        mFlushMode                        = std::get<1>(GetParam());
        mContextMode                      = std::get<2>(GetParam());

        std::vector<const char *> enabledFeatureOverrides;
        switch (mFlushMode)
        {
            case FlushMode::GlSyncImplicitSubmitEnabled:
                // noop
                break;
            case FlushMode::ExplicitGlFlushWithGlSyncImplicitSubmitDisabled:
            case FlushMode::GlClientWaitFlagFlushWithGlSyncImplicitSubmitDisabled:
            case FlushMode::GlSyncImplicitSubmitDisabled:
                enabledFeatureOverrides.push_back(
                    "disableSubmitCommandsOnSyncStatusCheckForTesting");
                break;
        }
        enabledFeatureOverrides.push_back(nullptr);

        if (!IsEGLClientExtensionEnabled("EGL_ANGLE_feature_control"))
        {
            GTEST_SKIP() << "Test skipped because EGL_ANGLE_feature_control is not available.";
        }
        EGLAttrib dispattrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, platformParams.getRenderer(),
                                 EGL_FEATURE_OVERRIDES_ENABLED_ANGLE,
                                 reinterpret_cast<EGLAttrib>(enabledFeatureOverrides.data()),
                                 EGL_NONE};

        mDisplay = eglGetPlatformDisplay(GetEglPlatform(),
                                         reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
        EXPECT_NE(mDisplay, EGL_NO_DISPLAY);
        EXPECT_EGL_TRUE(eglInitialize(mDisplay, nullptr, nullptr));

        chooseConfig(&mConfig);
        EXPECT_NE(mConfig, EGL_NO_CONFIG_KHR);

        createContext(mConfig, &mContexts[0]);
        createPbufferSurface(mConfig, &mSurfaces[0]);
        if (mContextMode == ContextMode::MultiContext)
        {
            createContext(mConfig, &mContexts[1]);
            createPbufferSurface(mConfig, &mSurfaces[1]);
        }

        EXPECT_TRUE(eglMakeCurrent(mDisplay, mSurfaces[0], mSurfaces[0], mContexts[0]));
        EXPECT_EGL_SUCCESS() << "eglMakeCurrent failed.";
    }

    void testTearDown() override
    {
        if (mDisplay != EGL_NO_DISPLAY)
        {
            eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            for (size_t i = 0; i < 2; i++)
            {
                if (mSurfaces[i] != EGL_NO_SURFACE)
                {
                    eglDestroySurface(mDisplay, mSurfaces[i]);
                }
                if (mContexts[i] != EGL_NO_CONTEXT)
                {
                    eglDestroyContext(mDisplay, mContexts[i]);
                }
            }
            eglTerminate(mDisplay);
            eglReleaseThread();
            mDisplay = EGL_NO_DISPLAY;
        }
        EXPECT_EGL_SUCCESS() << "Error during test TearDown";
    }

    void chooseConfig(EGLConfig *config)
    {
        EGLint attribs[] = {EGL_RED_SIZE,
                            8,
                            EGL_GREEN_SIZE,
                            8,
                            EGL_BLUE_SIZE,
                            8,
                            EGL_ALPHA_SIZE,
                            8,
                            EGL_RENDERABLE_TYPE,
                            EGL_OPENGL_ES3_BIT,
                            EGL_SURFACE_TYPE,
                            EGL_PBUFFER_BIT,
                            EGL_BIND_TO_TEXTURE_RGBA,
                            EGL_TRUE,
                            EGL_NONE};

        EGLint count = 0;
        EXPECT_TRUE(eglChooseConfig(mDisplay, attribs, config, 1, &count));
        EXPECT_EGL_TRUE(count > 0);
    }

    void createContext(EGLConfig config, EGLContext *context)
    {
        ASSERT(context);
        EGLint attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE};

        *context = eglCreateContext(mDisplay, config, nullptr, attribs);
        EXPECT_EGL_SUCCESS();
        EXPECT_NE(*context, EGL_NO_CONTEXT);
    }

    void createPbufferSurface(EGLConfig config, EGLSurface *surface)
    {
        ASSERT(surface);
        EGLint attribs[] = {EGL_WIDTH,
                            kWidth,
                            EGL_HEIGHT,
                            kHeight,
                            EGL_TEXTURE_FORMAT,
                            EGL_TEXTURE_RGBA,
                            EGL_TEXTURE_TARGET,
                            EGL_TEXTURE_2D,
                            EGL_NONE};

        *surface = eglCreatePbufferSurface(mDisplay, config, attribs);
        EXPECT_NE(*surface, EGL_NO_SURFACE);
    }

    EGLDisplay mDisplay     = EGL_NO_DISPLAY;
    EGLContext mContexts[2] = {EGL_NO_CONTEXT, EGL_NO_CONTEXT};
    EGLContext mSurfaces[2] = {EGL_NO_SURFACE, EGL_NO_SURFACE};
    EGLConfig mConfig       = EGL_NO_CONFIG_KHR;
    FlushMode mFlushMode;
    ContextMode mContextMode;
    static const EGLint kWidth  = 16;
    static const EGLint kHeight = 16;
};

// Test eglBindTexImage with different parameters
TEST_P(EGLBindTexImageTest, Basic)
{
    // Draw to mSurfaces[0]
    GLuint program = CompileProgram(essl3_shaders::vs::Passthrough(), essl3_shaders::fs::Red());
    EXPECT_GL_NO_ERROR();
    glUseProgram(program);
    EXPECT_GL_NO_ERROR();
    drawQuad(program, "a_position", 0.5f);
    EXPECT_GL_NO_ERROR();

    GLsync sync;
    if (mContextMode == ContextMode::SingleContext)
    {
        sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        EXPECT_GL_NO_ERROR();
    }
    else
    {
        EXPECT_TRUE(eglMakeCurrent(mDisplay, mSurfaces[1], mSurfaces[1], mContexts[1]));
        EXPECT_EGL_SUCCESS() << "eglMakeCurrent failed.";
    }

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    EXPECT_GL_NO_ERROR();

    // Bind mSurfaces[0] as texture
    eglBindTexImage(mDisplay, mSurfaces[0], EGL_BACK_BUFFER);
    EXPECT_EGL_SUCCESS();

    if (mFlushMode == FlushMode::ExplicitGlFlushWithGlSyncImplicitSubmitDisabled)
    {
        glFlush();
    }
    EXPECT_GL_NO_ERROR();

    if (mContextMode == ContextMode::SingleContext)
    {
        // For single contexts, check that prior commands have been submitted by
        // creating and client waiting on a sync object and expecting to be unblocked.
        GLbitfield clientWaitSyncFlags =
            mFlushMode == FlushMode::GlClientWaitFlagFlushWithGlSyncImplicitSubmitDisabled
                ? GL_SYNC_FLUSH_COMMANDS_BIT
                : 0;

        constexpr GLuint64 kNanosPerSecond = 1000000000;

        EGLint syncStatus = glClientWaitSync(sync, clientWaitSyncFlags, kNanosPerSecond);
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(syncStatus == GL_CONDITION_SATISFIED || syncStatus == GL_ALREADY_SIGNALED);
    }
    else
    {
        // For multiple contexts, bind the texture to a framebuffer and read from that
        GLFramebuffer fb;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
        EXPECT_GL_NO_ERROR();

        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        EXPECT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kWidth / 2, kHeight / 2, angle::GLColor::red);
        EXPECT_GL_NO_ERROR();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        EXPECT_GL_NO_ERROR();
    }

    eglReleaseTexImage(mDisplay, mSurfaces[0], EGL_BACK_BUFFER);
    EXPECT_EGL_SUCCESS();
    glBindTexture(GL_TEXTURE_2D, 0);
    EXPECT_GL_NO_ERROR();
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLBindTexImageTest);
ANGLE_INSTANTIATE_TEST_COMBINE_2(
    EGLBindTexImageTest,
    PrintBindTexImageTestParamsToString,
    testing::Values(FlushMode::GlSyncImplicitSubmitEnabled,
                    FlushMode::ExplicitGlFlushWithGlSyncImplicitSubmitDisabled,
                    FlushMode::GlClientWaitFlagFlushWithGlSyncImplicitSubmitDisabled,
                    FlushMode::GlSyncImplicitSubmitDisabled),
    testing::Values(ContextMode::SingleContext, ContextMode::MultiContext),
    WithNoFixture(ES3_VULKAN()));
