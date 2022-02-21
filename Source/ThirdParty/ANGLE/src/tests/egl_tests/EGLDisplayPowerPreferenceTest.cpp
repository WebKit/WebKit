//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLPowerPreferenceTest.cpp:
//   EGL extension EGL_ANGLE_display_power_preference
//

#include <gtest/gtest.h>

#include "common/debug.h"
#include "gpu_info_util/SystemInfo.h"
#include "test_utils/ANGLETest.h"
#include "util/OSWindow.h"

using namespace angle;

class EGLDisplayPowerPreferenceTest : public ANGLETest
{
  public:
    void testSetUp() override { (void)GetSystemInfo(&mSystemInfo); }

  protected:
    // Returns the index of the low or high power GPU in SystemInfo depending on the argument.
    int findGPU(bool lowPower)
    {
        if (mSystemInfo.gpus.size() < 2)
        {
            return 0;
        }
        for (int i = 0; i < static_cast<int>(mSystemInfo.gpus.size()); ++i)
        {
            if (lowPower && IsIntel(mSystemInfo.gpus[i].vendorId))
            {
                return i;
            }
            // Return the high power GPU, i.e any non-intel GPU
            else if (!lowPower && !IsIntel(mSystemInfo.gpus[i].vendorId))
            {
                return i;
            }
        }
        ASSERT(false);
        return 0;
    }

    // Returns the index of the active GPU in SystemInfo based on the renderer string.
    int findActiveGPU()
    {
        char *renderer = (char *)glGetString(GL_RENDERER);
        std::string rendererString(renderer);
        for (int i = 0; i < static_cast<int>(mSystemInfo.gpus.size()); ++i)
        {
            if (rendererString.find(VendorName(mSystemInfo.gpus[i].vendorId)) != std::string::npos)
            {
                return i;
            }
        }
        return 0;
    }

    SystemInfo mSystemInfo;
};

class EGLDisplayPowerPreferenceTestMultiDisplay : public EGLDisplayPowerPreferenceTest
{
  protected:
    void terminateWindow()
    {
        if (mOSWindow)
        {
            OSWindow::Delete(&mOSWindow);
        }
    }

    void terminateDisplay(EGLDisplay display)
    {
        // EXPECT_EGL_TRUE(eglTerminate(display));
        // EXPECT_EGL_SUCCESS();
    }

    void terminateContext(EGLDisplay display, EGLContext context)
    {
        if (context != EGL_NO_CONTEXT)
        {
            eglDestroyContext(display, context);
            ASSERT_EGL_SUCCESS();
        }
    }

    void initializeWindow()
    {
        mOSWindow = OSWindow::New();
        mOSWindow->initialize("EGLDisplayPowerPreferenceTestMultiDisplay", kWindowWidth,
                              kWindowHeight);
        setWindowVisible(mOSWindow, true);
    }

    void initializeDisplayWithPowerPreference(EGLDisplay *display, EGLAttrib powerPreference)
    {
        GLenum platformType = GetParam().getRenderer();
        GLenum deviceType   = GetParam().getDeviceType();

        std::vector<EGLint> displayAttributes;
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
        displayAttributes.push_back(platformType);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE);
        displayAttributes.push_back(EGL_DONT_CARE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE);
        displayAttributes.push_back(EGL_DONT_CARE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
        displayAttributes.push_back(deviceType);
        displayAttributes.push_back(EGL_POWER_PREFERENCE_ANGLE);
        displayAttributes.push_back(powerPreference);
        displayAttributes.push_back(EGL_NONE);

        *display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE,
                                            reinterpret_cast<void *>(mOSWindow->getNativeDisplay()),
                                            displayAttributes.data());
        ASSERT_TRUE(*display != EGL_NO_DISPLAY);

        EGLint majorVersion, minorVersion;
        ASSERT_TRUE(eglInitialize(*display, &majorVersion, &minorVersion) == EGL_TRUE);

        eglBindAPI(EGL_OPENGL_ES_API);
        ASSERT_EGL_SUCCESS();
    }

    void initializeContextForDisplay(EGLDisplay display, EGLContext *context)
    {
        // Find a default config.
        const EGLint configAttributes[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE,     EGL_DONT_CARE,  EGL_GREEN_SIZE,
            EGL_DONT_CARE,    EGL_BLUE_SIZE,  EGL_DONT_CARE,    EGL_ALPHA_SIZE, EGL_DONT_CARE,
            EGL_DEPTH_SIZE,   EGL_DONT_CARE,  EGL_STENCIL_SIZE, EGL_DONT_CARE,  EGL_NONE};

        EGLint configCount;
        EGLConfig config;
        EGLint ret = eglChooseConfig(display, configAttributes, &config, 1, &configCount);

        if (!ret || configCount == 0)
        {
            return;
        }

        EGLint contextAttributes[] = {
            EGL_CONTEXT_MAJOR_VERSION_KHR,
            GetParam().majorVersion,
            EGL_CONTEXT_MINOR_VERSION_KHR,
            GetParam().minorVersion,
            EGL_NONE,
        };

        *context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
        ASSERT_TRUE(*context != EGL_NO_CONTEXT);
    }

    void runReinitializeDisplay(EGLAttrib powerPreference)
    {
        initializeWindow();

        // Initialize the display with the selected power preferenc
        EGLDisplay display;
        EGLContext context;
        initializeDisplayWithPowerPreference(&display, powerPreference);
        initializeContextForDisplay(display, &context);
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);

        bool lowPower = (powerPreference == EGL_LOW_POWER_ANGLE);
        ASSERT_EQ(findGPU(lowPower), findActiveGPU());

        // Terminate the display
        terminateContext(display, context);
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        terminateDisplay(display);

        // Change the power preference
        if (powerPreference == EGL_LOW_POWER_ANGLE)
        {
            powerPreference = EGL_HIGH_POWER_ANGLE;
        }
        else
        {
            powerPreference = EGL_LOW_POWER_ANGLE;
        }

        // Reinitialize the display with a new power preference
        initializeDisplayWithPowerPreference(&display, powerPreference);
        initializeContextForDisplay(display, &context);
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);

        // Expect that the power preference has changed
        lowPower = (powerPreference == EGL_LOW_POWER_ANGLE);
        ASSERT_EQ(findGPU(lowPower), findActiveGPU());

        // Terminate the display
        terminateContext(display, context);
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        terminateDisplay(display);

        terminateWindow();
    }

    void runMultiDisplay()
    {
        initializeWindow();

        // Initialize the first display with low power
        EGLDisplay display1;
        EGLContext context1;
        initializeDisplayWithPowerPreference(&display1, EGL_LOW_POWER_ANGLE);
        initializeContextForDisplay(display1, &context1);
        eglMakeCurrent(display1, EGL_NO_SURFACE, EGL_NO_SURFACE, context1);

        ASSERT_EQ(findGPU(true), findActiveGPU());

        // Initialize the second display with high power
        EGLDisplay display2;
        EGLContext context2;
        initializeDisplayWithPowerPreference(&display2, EGL_HIGH_POWER_ANGLE);
        initializeContextForDisplay(display2, &context2);
        eglMakeCurrent(display2, EGL_NO_SURFACE, EGL_NO_SURFACE, context2);

        ASSERT_EQ(findGPU(false), findActiveGPU());

        // Switch back to the first display to verify
        eglMakeCurrent(display1, EGL_NO_SURFACE, EGL_NO_SURFACE, context1);
        ASSERT_EQ(findGPU(true), findActiveGPU());

        // Terminate the displays
        terminateContext(display1, context1);
        eglMakeCurrent(display1, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        terminateDisplay(display1);
        terminateContext(display2, context2);
        eglMakeCurrent(display2, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        terminateDisplay(display2);

        terminateWindow();
    }

    static constexpr int kWindowWidth  = 16;
    static constexpr int kWindowHeight = 8;

    OSWindow *mOSWindow = nullptr;
};

TEST_P(EGLDisplayPowerPreferenceTest, SelectGPU)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_display_power_preference"));
    ASSERT_NE(GetParam().eglParameters.displayPowerPreference, EGL_DONT_CARE);

    bool lowPower = (GetParam().eglParameters.displayPowerPreference == EGL_LOW_POWER_ANGLE);
    ASSERT_EQ(findGPU(lowPower), findActiveGPU());
}

TEST_P(EGLDisplayPowerPreferenceTestMultiDisplay, ReInitializePowerPreferenceLowToHigh)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_display_power_preference"));

    runReinitializeDisplay(EGL_LOW_POWER_ANGLE);
}

TEST_P(EGLDisplayPowerPreferenceTestMultiDisplay, ReInitializePowerPreferenceHighToLow)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_display_power_preference"));

    runReinitializeDisplay(EGL_HIGH_POWER_ANGLE);
}

TEST_P(EGLDisplayPowerPreferenceTestMultiDisplay, MultiDisplayTest)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_display_power_preference"));

    runMultiDisplay();
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLDisplayPowerPreferenceTest);
ANGLE_INSTANTIATE_TEST(EGLDisplayPowerPreferenceTest,
                       WithLowPowerGPU(ES2_METAL()),
                       WithLowPowerGPU(ES3_METAL()),
                       WithHighPowerGPU(ES2_METAL()),
                       WithHighPowerGPU(ES3_METAL()));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLDisplayPowerPreferenceTestMultiDisplay);
ANGLE_INSTANTIATE_TEST(EGLDisplayPowerPreferenceTestMultiDisplay,
                       WithNoFixture(ES2_METAL()),
                       WithNoFixture(ES3_METAL()));
