//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// EGLDeviceLossTest: Tests that a backend reports the loss of its device as a context reset,
// through GL_EXT_robustness, GL_KHR_robustness and EGL_CONTEXT_LOST. The tests lose the device with
// EGL_ANGLE_lose_device, which loses it as a GPU failure would, so the backend detects the loss
// as it detects a real one. GL_CHROMIUM_lose_context would only mark the context lost in the front
// end.

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "test_utils/ANGLETest.h"

#include <sstream>
#include <tuple>

using namespace angle;

namespace
{
// The platform, and the reset notification strategy of the contexts.
using DeviceLossTestParams = std::tuple<PlatformParameters, EGLint>;

std::string DeviceLossTestPrint(const ::testing::TestParamInfo<DeviceLossTestParams> &paramsInfo)
{
    const DeviceLossTestParams &params = paramsInfo.param;
    std::ostringstream out;
    out << std::get<0>(params) << "__"
        << (std::get<1>(params) == EGL_LOSE_CONTEXT_ON_RESET_EXT ? "LoseContextOnReset"
                                                                 : "NoResetNotification");
    return out.str();
}

// Each test runs on a new display, which creates its device.
class EGLDeviceLossTest : public ANGLETest<DeviceLossTestParams>
{
  protected:
    void testSetUp() override { initialize(); }

    void testTearDown() override { terminate(); }

    const PlatformParameters &platform() const { return std::get<0>(GetParam()); }
    EGLint resetStrategy() const { return std::get<1>(GetParam()); }
    bool notifiesReset() const { return resetStrategy() == EGL_LOSE_CONTEXT_ON_RESET_EXT; }

    void initialize()
    {
        const EGLAttrib displayAttributes[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                                               platform().getRenderer(), EGL_NONE};
        mDisplay =
            eglGetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE,
                                  reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), displayAttributes);
        ASSERT_NE(mDisplay, EGL_NO_DISPLAY);
        ASSERT_NO_FATAL_FAILURE(initializeDisplay());
    }

    void initializeDisplay()
    {
        ASSERT_EGL_TRUE(eglInitialize(mDisplay, nullptr, nullptr));
        ASSERT_TRUE(IsEGLDisplayExtensionEnabled(mDisplay, "EGL_ANGLE_lose_device"));
        ASSERT_TRUE(IsEGLDisplayExtensionEnabled(mDisplay, "EGL_EXT_create_context_robustness"));

        const EGLint configAttributes[] = {
            EGL_SURFACE_TYPE,
            EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE,
            platform().majorVersion >= 3 ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE,
            8,
            EGL_NONE};
        EGLint configCount = 0;
        ASSERT_EGL_TRUE(eglChooseConfig(mDisplay, configAttributes, &mConfig, 1, &configCount));
        ASSERT_EQ(configCount, 1);

        mContext = createContext(EGL_NO_CONTEXT);
        ASSERT_NE(mContext, EGL_NO_CONTEXT);
        ASSERT_TRUE(IsEGLDisplayExtensionEnabled(mDisplay, "EGL_KHR_surfaceless_context"));
        ASSERT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, mContext));
        glGenTextures(1, &mTexture);
        glBindTexture(GL_TEXTURE_2D, mTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenFramebuffers(1, &mFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture, 0);
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        // The contexts have both robustness extensions. The test asks for the reset status with
        // GL_KHR_robustness; ANGLE implements both extensions with the same reset status.
        ASSERT_TRUE(IsGLExtensionEnabled("GL_EXT_robustness"));
        ASSERT_TRUE(IsGLExtensionEnabled("GL_KHR_robustness"));
        ASSERT_GL_NO_ERROR();
        EXPECT_GLENUM_EQ(GL_NO_ERROR, glGetGraphicsResetStatusKHR());
    }

    void terminate()
    {
        if (mDisplay != EGL_NO_DISPLAY)
        {
            EXPECT_EGL_TRUE(
                eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
            EXPECT_EGL_TRUE(eglTerminate(mDisplay));
        }
        mContext     = EGL_NO_CONTEXT;
        mTexture     = 0;
        mFramebuffer = 0;
        mDisplay     = EGL_NO_DISPLAY;
    }

    EGLContext createContext(EGLContext shareContext)
    {
        const EGLint attributes[] = {EGL_CONTEXT_MAJOR_VERSION,
                                     platform().majorVersion,
                                     EGL_CONTEXT_MINOR_VERSION,
                                     platform().minorVersion,
                                     EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT,
                                     resetStrategy(),
                                     EGL_NONE};
        return eglCreateContext(mDisplay, mConfig, shareContext, attributes);
    }

    // The status that a context reports once it has seen the loss. The backends do not say which
    // context caused the loss, and the status stays the same.
    GLenum lostStatus() const { return notifiesReset() ? GL_UNKNOWN_CONTEXT_RESET : GL_NO_ERROR; }

    void loseDevice()
    {
        ASSERT_EGL_TRUE(eglLoseDeviceANGLE(mDisplay));
        ASSERT_EGL_SUCCESS();
    }

    // The current context has seen the loss.
    void expectLost()
    {
        EXPECT_GLENUM_EQ(lostStatus(), glGetGraphicsResetStatusKHR());
        EXPECT_GLENUM_EQ(lostStatus(), glGetGraphicsResetStatusKHR());

        // The loss is not reported as an internal error.
        for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
        {
            EXPECT_GLENUM_EQ(GL_CONTEXT_LOST, error);
        }
        if (notifiesReset())
        {
            glBindTexture(GL_TEXTURE_2D, 0);
            EXPECT_GL_ERROR(GL_CONTEXT_LOST);
        }

        expectDisplayCreatesNothing();

        EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        if (eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, mContext))
        {
            // Whether a lost context can be made current again is up to the implementation. If it
            // can, it still reports the loss.
            EXPECT_GLENUM_EQ(lostStatus(), glGetGraphicsResetStatusKHR());
        }
    }

    // A display that has lost its device creates nothing more. A context that asks for reset
    // notifications has to be destroyed first; otherwise the lost device cannot be restored.
    void expectDisplayCreatesNothing()
    {
        EXPECT_EQ(createContext(EGL_NO_CONTEXT), EGL_NO_CONTEXT);
        EXPECT_EGL_ERROR(notifiesReset() ? EGL_CONTEXT_LOST : EGL_BAD_DISPLAY);
    }

    // The loss that the current context sees is reported by another context of the display,
    // without it using the device.
    void expectOtherContextReportsLoss(EGLContext shareContext)
    {
        EGLContext otherContext = createContext(shareContext);
        ASSERT_NE(otherContext, EGL_NO_CONTEXT);
        clearAndFinish();
        ASSERT_NO_FATAL_FAILURE(loseDevice());
        glFinish();
        expectLost();

        ASSERT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, otherContext));
        EXPECT_GLENUM_EQ(lostStatus(), glGetGraphicsResetStatusKHR());
        EXPECT_GL_NO_ERROR();
        if (notifiesReset())
        {
            glBindTexture(GL_TEXTURE_2D, 0);
            EXPECT_GL_ERROR(GL_CONTEXT_LOST);
        }
    }

    void clearAndFinish()
    {
        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glFinish();
        EXPECT_GL_NO_ERROR();
    }

    EGLDisplay mDisplay = EGL_NO_DISPLAY;
    EGLConfig mConfig   = nullptr;
    GLuint mTexture     = 0;
    GLuint mFramebuffer = 0;
    EGLContext mContext = EGL_NO_CONTEXT;
};

// A device that is lost before any work is seen at glFinish().
TEST_P(EGLDeviceLossTest, LossBeforeAnyWorkIsSeenAtFinish)
{
    ASSERT_NO_FATAL_FAILURE(loseDevice());
    glFinish();
    expectLost();
}

// A device that is lost between draws is seen when the draws are submitted.
TEST_P(EGLDeviceLossTest, LossBetweenDrawsIsSeenAtFlush)
{
    clearAndFinish();
    ASSERT_NO_FATAL_FAILURE(loseDevice());
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();
    expectLost();
}

// A device that is lost before glReadPixels() is seen by glReadPixels().
TEST_P(EGLDeviceLossTest, LossIsSeenAtReadPixels)
{
    clearAndFinish();
    ASSERT_NO_FATAL_FAILURE(loseDevice());
    GLColor color;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &color);
    expectLost();
}

// EGL sees the loss when it creates an object, and the contexts of the display report it.
TEST_P(EGLDeviceLossTest, LossIsSeenByEGL)
{
    clearAndFinish();
    ASSERT_NO_FATAL_FAILURE(loseDevice());
    expectDisplayCreatesNothing();
    expectLost();
}

// A device that is lost again stays lost.
TEST_P(EGLDeviceLossTest, LosingTheDeviceAgainKeepsItLost)
{
    clearAndFinish();
    ASSERT_NO_FATAL_FAILURE(loseDevice());
    glFinish();
    ASSERT_NO_FATAL_FAILURE(loseDevice());
    glFinish();
    expectLost();
}

// A display whose device is lost works again once it is terminated and initialized again, with a
// new device.
TEST_P(EGLDeviceLossTest, DisplayWorksAfterItIsInitializedAgain)
{
    clearAndFinish();
    ASSERT_NO_FATAL_FAILURE(loseDevice());
    glFinish();
    expectLost();

    // The same display, without eglGetPlatformDisplay() in between.
    EGLDisplay display = mDisplay;
    terminate();
    mDisplay = display;
    ASSERT_NO_FATAL_FAILURE(initializeDisplay());
    clearAndFinish();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_GLENUM_EQ(GL_NO_ERROR, glGetGraphicsResetStatusKHR());
}

// The loss is reported by a context in the share group of the context that sees it.
TEST_P(EGLDeviceLossTest, LossIsReportedByContextInShareGroup)
{
    expectOtherContextReportsLoss(mContext);
}

// The loss is reported by a context outside the share group of the context that sees it.
TEST_P(EGLDeviceLossTest, LossIsReportedByContextOutsideShareGroup)
{
    expectOtherContextReportsLoss(EGL_NO_CONTEXT);
}

// Tests the errors of eglLoseDeviceANGLE().
class EGLLoseDeviceTest : public ANGLETest<>
{};

// eglLoseDeviceANGLE() needs an initialized display.
TEST_P(EGLLoseDeviceTest, NeedsAnInitializedDisplay)
{
    EXPECT_EGL_FALSE(eglLoseDeviceANGLE(EGL_NO_DISPLAY));
    EXPECT_EGL_ERROR(EGL_BAD_DISPLAY);

    const EGLAttrib displayAttributes[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, GetParam().getRenderer(),
                                           EGL_NONE};
    EGLDisplay display                  = eglGetPlatformDisplay(
        EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), displayAttributes);
    ASSERT_NE(display, EGL_NO_DISPLAY);
    EXPECT_EGL_FALSE(eglLoseDeviceANGLE(display));
    EXPECT_EGL_ERROR(EGL_NOT_INITIALIZED);
}

constexpr EGLint kResetStrategies[] = {EGL_LOSE_CONTEXT_ON_RESET_EXT,
                                       EGL_NO_RESET_NOTIFICATION_EXT};

}  // anonymous namespace

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLDeviceLossTest);
ANGLE_INSTANTIATE_TEST_COMBINE_1(EGLDeviceLossTest,
                                 DeviceLossTestPrint,
                                 testing::ValuesIn(kResetStrategies),
                                 WithNoFixture(ES2_METAL()),
                                 WithNoFixture(ES3_METAL()),
                                 WithNoFixture(ES2_WEBGPU()),
                                 WithNoFixture(ES3_WEBGPU()));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLLoseDeviceTest);
ANGLE_INSTANTIATE_TEST(EGLLoseDeviceTest,
                       WithNoFixture(ES2_METAL()),
                       WithNoFixture(ES3_METAL()),
                       WithNoFixture(ES2_WEBGPU()),
                       WithNoFixture(ES3_WEBGPU()));
