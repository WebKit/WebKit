//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLDisplaySelectionTest.cpp:
//   Checks display selection and caching with EGL extensions EGL_ANGLE_display_power_preference,
//   EGL_ANGLE_platform_angle, and EGL_ANGLE_device_id
//

#include <gtest/gtest.h>

#include <array>
#include <set>

#include "common/debug.h"
#include "common/string_utils.h"
#include "gpu_info_util/SystemInfo.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/system_info_util.h"
#include "util/OSWindow.h"

using namespace angle;

class EGLDisplaySelectionTest : public ANGLETest<>
{
  public:
    void testSetUp() override { (void)GetSystemInfo(&mSystemInfo); }

  protected:
    // Returns the index of the low or high power GPU in SystemInfo depending on the argument.
    int findGPU(bool lowPower) const
    {
        if (lowPower)
        {
            return FindLowPowerGPU(mSystemInfo);
        }
        return FindHighPowerGPU(mSystemInfo);
    }

    // Returns the index of the active GPU in SystemInfo based on the renderer string.
    int findActiveGPU() const { return FindActiveOpenGLGPU(mSystemInfo); }

    SystemInfo mSystemInfo;
};

class EGLDisplaySelectionTestNoFixture : public EGLDisplaySelectionTest
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
        EXPECT_EGL_TRUE(eglTerminate(display));
        EXPECT_EGL_SUCCESS();
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
        mOSWindow->initialize("EGLDisplaySelectionTestMultiDisplay", kWindowWidth, kWindowHeight);
        setWindowVisible(mOSWindow, shouldShowWindow());
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

    static constexpr int kWindowWidth  = 16;
    static constexpr int kWindowHeight = 8;

    OSWindow *mOSWindow = nullptr;
};

class EGLDisplaySelectionTestMultiDisplay : public EGLDisplaySelectionTestNoFixture
{

  protected:
    void initializeDisplayWithPowerPreference(EGLDisplay *display, EGLAttrib powerPreference)
    {
        GLenum platformType = GetParam().getRenderer();
        GLenum deviceType   = GetParam().getDeviceType();

        std::vector<EGLAttrib> displayAttributes;
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

        *display = eglGetPlatformDisplay(GetEglPlatform(),
                                         reinterpret_cast<void *>(mOSWindow->getNativeDisplay()),
                                         displayAttributes.data());
        ASSERT_TRUE(*display != EGL_NO_DISPLAY);

        EGLint majorVersion, minorVersion;
        ASSERT_TRUE(eglInitialize(*display, &majorVersion, &minorVersion) == EGL_TRUE);

        eglBindAPI(EGL_OPENGL_ES_API);
        ASSERT_EGL_SUCCESS();
    }

    void initializeDisplayWithBackend(EGLDisplay *display, EGLAttrib platformType)
    {
        GLenum deviceType = GetParam().getDeviceType();

        std::vector<EGLAttrib> displayAttributes;
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
        displayAttributes.push_back(platformType);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE);
        displayAttributes.push_back(EGL_DONT_CARE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE);
        displayAttributes.push_back(EGL_DONT_CARE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
        displayAttributes.push_back(deviceType);
        displayAttributes.push_back(EGL_NONE);

        *display = eglGetPlatformDisplay(GetEglPlatform(),
                                         reinterpret_cast<void *>(mOSWindow->getNativeDisplay()),
                                         displayAttributes.data());
        ASSERT_TRUE(*display != EGL_NO_DISPLAY);

        EGLint majorVersion, minorVersion;
        ASSERT_TRUE(eglInitialize(*display, &majorVersion, &minorVersion) == EGL_TRUE);

        eglBindAPI(EGL_OPENGL_ES_API);
        ASSERT_EGL_SUCCESS();
    }

    void runReinitializeDisplayPowerPreference(EGLAttrib powerPreference)
    {
        initializeWindow();

        // Initialize the display with the selected power preference
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

    void runMultiDisplayBackend(EGLAttrib backend1,
                                bool(checkFunc1)(void),
                                EGLAttrib backend2,
                                bool(checkFunc2)(void))
    {
        initializeWindow();

        // Initialize the display with the selected backend
        EGLDisplay display1;
        EGLContext context1;
        initializeDisplayWithBackend(&display1, backend1);
        initializeContextForDisplay(display1, &context1);
        eglMakeCurrent(display1, EGL_NO_SURFACE, EGL_NO_SURFACE, context1);

        // Check that the correct backend is chosen
        ASSERT_TRUE(checkFunc1());

        // Initialize the second display with the second backend
        EGLDisplay display2;
        EGLContext context2;
        initializeDisplayWithBackend(&display2, backend2);
        initializeContextForDisplay(display2, &context2);
        eglMakeCurrent(display2, EGL_NO_SURFACE, EGL_NO_SURFACE, context2);

        // Check that the correct backend is chosen
        ASSERT_TRUE(checkFunc2());

        // Switch back to the first display to verify
        eglMakeCurrent(display1, EGL_NO_SURFACE, EGL_NO_SURFACE, context1);
        ASSERT_TRUE(checkFunc1());

        // Terminate the displays
        terminateContext(display1, context1);
        eglMakeCurrent(display1, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        terminateDisplay(display1);
        terminateContext(display2, context2);
        eglMakeCurrent(display2, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        terminateDisplay(display2);

        terminateWindow();
    }

    void runMultiDisplayBackendDefault(EGLAttrib backend, bool(checkFunc)(void))
    {
        initializeWindow();

        // Initialize the display with the selected backend
        EGLDisplay display1;
        EGLContext context1;
        initializeDisplayWithBackend(&display1, backend);
        initializeContextForDisplay(display1, &context1);
        eglMakeCurrent(display1, EGL_NO_SURFACE, EGL_NO_SURFACE, context1);

        // Check that the correct backend is chosen
        ASSERT_TRUE(checkFunc());

        // Initialize the second display with the second backend
        EGLDisplay display2;
        EGLContext context2;
        initializeDisplayWithBackend(&display2, EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE);
        initializeContextForDisplay(display2, &context2);
        eglMakeCurrent(display2, EGL_NO_SURFACE, EGL_NO_SURFACE, context2);

        bool sameDisplay = false;
        // If this backend is the same as the first display, check that the display is cached
        if (checkFunc())
        {
            ASSERT_EQ(display1, display2);
            sameDisplay = true;
        }
        // If this backend is not the same, check that this is a different display
        else
        {
            ASSERT_NE(display1, display2);
        }

        // Switch back to the first display to verify
        eglMakeCurrent(display1, EGL_NO_SURFACE, EGL_NO_SURFACE, context1);
        ASSERT_TRUE(checkFunc());

        // Terminate the displays
        terminateContext(display1, context1);
        eglMakeCurrent(display1, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (!sameDisplay)
        {
            terminateDisplay(display1);
        }
        terminateContext(display2, context2);
        eglMakeCurrent(display2, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        terminateDisplay(display2);

        terminateWindow();
    }

    void runMultiDisplayPowerPreference()
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
};

TEST_P(EGLDisplaySelectionTest, SelectGPU)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_display_power_preference"));
    ASSERT_NE(GetParam().eglParameters.displayPowerPreference, EGL_DONT_CARE);

    bool lowPower = (GetParam().eglParameters.displayPowerPreference == EGL_LOW_POWER_ANGLE);
    ASSERT_EQ(findGPU(lowPower), findActiveGPU());
}

TEST_P(EGLDisplaySelectionTestMultiDisplay, ReInitializePowerPreferenceLowToHigh)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_display_power_preference"));

    runReinitializeDisplayPowerPreference(EGL_LOW_POWER_ANGLE);
}

TEST_P(EGLDisplaySelectionTestMultiDisplay, ReInitializePowerPreferenceHighToLow)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_display_power_preference"));

    runReinitializeDisplayPowerPreference(EGL_HIGH_POWER_ANGLE);
}

TEST_P(EGLDisplaySelectionTestMultiDisplay, BackendMetalOpenGL)
{
    bool missingBackends = true;
#if defined(ANGLE_ENABLE_METAL) && defined(ANGLE_ENABLE_OPENGL)
    missingBackends = false;
#endif
    ANGLE_SKIP_TEST_IF(missingBackends);

    runMultiDisplayBackend(EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE, IsMetal,
                           EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE, IsOpenGL);
}

TEST_P(EGLDisplaySelectionTestMultiDisplay, BackendOpenGLMetal)
{
    bool missingBackends = true;
#if defined(ANGLE_ENABLE_METAL) && defined(ANGLE_ENABLE_OPENGL)
    missingBackends = false;
#endif
    ANGLE_SKIP_TEST_IF(missingBackends);

    runMultiDisplayBackend(EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE, IsOpenGL,
                           EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE, IsMetal);
}

TEST_P(EGLDisplaySelectionTestMultiDisplay, BackendVulkanD3D11)
{
    bool missingBackends = true;
#if defined(ANGLE_ENABLE_VULKAN) && defined(ANGLE_ENABLE_D3D11)
    missingBackends = false;
#endif
    ANGLE_SKIP_TEST_IF(missingBackends);

    runMultiDisplayBackend(EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE, IsVulkan,
                           EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, IsD3D11);
}

TEST_P(EGLDisplaySelectionTestMultiDisplay, BackendD3D11Vulkan)
{
    bool missingBackends = true;
#if defined(ANGLE_ENABLE_VULKAN) && defined(ANGLE_ENABLE_D3D11)
    missingBackends = false;
#endif
    ANGLE_SKIP_TEST_IF(missingBackends);

    runMultiDisplayBackend(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, IsD3D11,
                           EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE, IsVulkan);
}

TEST_P(EGLDisplaySelectionTestMultiDisplay, BackendDefaultMetal)
{
    bool missingBackends = true;
#if defined(ANGLE_ENABLE_METAL)
    missingBackends = false;
#endif
    ANGLE_SKIP_TEST_IF(missingBackends);

    runMultiDisplayBackendDefault(EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE, IsMetal);
}

TEST_P(EGLDisplaySelectionTestMultiDisplay, BackendDefaultOpenGL)
{
    bool missingBackends = true;
#if defined(ANGLE_ENABLE_OPENGL)
    missingBackends = false;
#endif
    ANGLE_SKIP_TEST_IF(missingBackends);

    runMultiDisplayBackendDefault(EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE, IsOpenGL);
}

TEST_P(EGLDisplaySelectionTestMultiDisplay, BackendDefaultD3D11)
{
    bool missingBackends = true;
#if defined(ANGLE_ENABLE_D3D11)
    missingBackends = false;
#endif
    ANGLE_SKIP_TEST_IF(missingBackends);

    runMultiDisplayBackendDefault(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, IsD3D11);
}

TEST_P(EGLDisplaySelectionTestMultiDisplay, BackendDefaultVulkan)
{
    bool missingBackends = true;
#if defined(ANGLE_ENABLE_VULKAN)
    missingBackends = false;
#endif
    ANGLE_SKIP_TEST_IF(missingBackends);

    // http://anglebug.com/42265471
    ANGLE_SKIP_TEST_IF(IsMac());

    runMultiDisplayBackendDefault(EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE, IsVulkan);
}

TEST_P(EGLDisplaySelectionTestMultiDisplay, PowerPreference)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_display_power_preference"));

    runMultiDisplayPowerPreference();
}

class EGLDisplaySelectionTestDeviceId : public EGLDisplaySelectionTestNoFixture
{

  protected:
    void initializeDisplayWithDeviceId(EGLDisplay *display, uint64_t deviceId)
    {
        GLenum platformType          = GetParam().getRenderer();
        GLenum deviceType            = GetParam().getDeviceType();
        GLint displayPowerPreference = GetParam().eglParameters.displayPowerPreference;

        EGLAttrib high = ((deviceId >> 32) & 0xFFFFFFFF);
        EGLAttrib low  = (deviceId & 0xFFFFFFFF);

        std::vector<EGLAttrib> displayAttributes;
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
        displayAttributes.push_back(platformType);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE);
        displayAttributes.push_back(EGL_DONT_CARE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE);
        displayAttributes.push_back(EGL_DONT_CARE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
        displayAttributes.push_back(deviceType);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEVICE_ID_HIGH_ANGLE);
        displayAttributes.push_back(high);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEVICE_ID_LOW_ANGLE);
        displayAttributes.push_back(low);
        if (displayPowerPreference != EGL_DONT_CARE)
        {
            displayAttributes.push_back(EGL_POWER_PREFERENCE_ANGLE);
            displayAttributes.push_back(displayPowerPreference);
        }
        displayAttributes.push_back(EGL_NONE);

        *display = eglGetPlatformDisplay(GetEglPlatform(),
                                         reinterpret_cast<void *>(mOSWindow->getNativeDisplay()),
                                         displayAttributes.data());
        ASSERT_TRUE(*display != EGL_NO_DISPLAY);

        EGLint majorVersion, minorVersion;
        ASSERT_TRUE(eglInitialize(*display, &majorVersion, &minorVersion) == EGL_TRUE);

        eglBindAPI(EGL_OPENGL_ES_API);
        ASSERT_EGL_SUCCESS();
    }
};

TEST_P(EGLDisplaySelectionTestDeviceId, DeviceId)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_platform_angle_device_id"));
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_display_power_preference") &&
                       GetParam().eglParameters.displayPowerPreference != EGL_DONT_CARE);

    initializeWindow();

    for (size_t i = 0; i < mSystemInfo.gpus.size(); i++)
    {
        // Initialize the display with device id for each GPU
        EGLDisplay display;
        EGLContext context;
        initializeDisplayWithDeviceId(&display, mSystemInfo.gpus[i].systemDeviceId);
        initializeContextForDisplay(display, &context);
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);

        ASSERT_EQ(static_cast<int>(i), findActiveGPU());

        // Terminate the displays
        terminateContext(display, context);
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        terminateDisplay(display);
    }

    terminateWindow();
}

TEST_P(EGLDisplaySelectionTestDeviceId, DeviceIdConcurrently)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_platform_angle_device_id"));
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_display_power_preference") &&
                       GetParam().eglParameters.displayPowerPreference != EGL_DONT_CARE);

    initializeWindow();
    struct ContextForDevice
    {
        EGLDisplay display{};
        EGLContext context{};
    };
    std::vector<ContextForDevice> contexts(mSystemInfo.gpus.size());

    for (size_t i = 0; i < contexts.size(); i++)
    {
        auto &display = contexts[i].display;
        auto &context = contexts[i].context;
        initializeDisplayWithDeviceId(&display, mSystemInfo.gpus[i].systemDeviceId);
        initializeContextForDisplay(display, &context);
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
        ASSERT_EQ(static_cast<int>(i), findActiveGPU());
    }
    for (auto &context : contexts)
    {
        // Terminate the displays
        terminateContext(context.display, context.context);
        eglMakeCurrent(context.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        terminateDisplay(context.display);
    }

    terminateWindow();
}

class EGLDisplaySelectionTestDisplayKey : public EGLDisplaySelectionTestNoFixture
{

  protected:
    void initializeDisplayWithKey(EGLDisplay *display, EGLAttrib displayKey)
    {
        GLenum platformType          = GetParam().getRenderer();
        GLenum deviceType            = GetParam().getDeviceType();
        GLint displayPowerPreference = GetParam().eglParameters.displayPowerPreference;

        std::vector<EGLAttrib> displayAttributes;
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
        displayAttributes.push_back(platformType);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE);
        displayAttributes.push_back(EGL_DONT_CARE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE);
        displayAttributes.push_back(EGL_DONT_CARE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
        displayAttributes.push_back(deviceType);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_DISPLAY_KEY_ANGLE);
        displayAttributes.push_back(displayKey);
        if (displayPowerPreference != EGL_DONT_CARE)
        {
            displayAttributes.push_back(EGL_POWER_PREFERENCE_ANGLE);
            displayAttributes.push_back(displayPowerPreference);
        }
        displayAttributes.push_back(EGL_NONE);

        *display = eglGetPlatformDisplay(GetEglPlatform(),
                                         reinterpret_cast<void *>(mOSWindow->getNativeDisplay()),
                                         displayAttributes.data());
        ASSERT_TRUE(*display != EGL_NO_DISPLAY);

        EGLint majorVersion, minorVersion;
        ASSERT_TRUE(eglInitialize(*display, &majorVersion, &minorVersion) == EGL_TRUE);

        eglBindAPI(EGL_OPENGL_ES_API);
        ASSERT_EGL_SUCCESS();
    }
};

// Test creating multiple displays with different display keys and verifying they are unique.
TEST_P(EGLDisplaySelectionTestDisplayKey, ConcurentDisplayKey)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_platform_angle_display_key"));

    initializeWindow();
    struct ContextForKey
    {
        EGLDisplay display{};
        EGLContext context{};
    };
    std::vector<ContextForKey> contexts;
    constexpr size_t kDisplayCount = 10;

    for (size_t i = 0; i < kDisplayCount; i++)
    {
        ContextForKey contextForKey;
        auto &display = contextForKey.display;
        auto &context = contextForKey.context;
        initializeDisplayWithKey(&display, i);
        initializeContextForDisplay(display, &context);
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);

        // Test that this display is unique
        for (auto &otherContext : contexts)
        {
            EXPECT_NE(display, otherContext.display);
        }

        contexts.push_back(contextForKey);
    }

    // Test that requesting using the same display key returns the same display
    for (size_t i = 0; i < kDisplayCount; i++)
    {
        EGLDisplay display = EGL_NO_DISPLAY;
        initializeDisplayWithKey(&display, i);
        EXPECT_EQ(contexts[i].display, display);
    }

    for (auto &context : contexts)
    {
        // Terminate the displays
        terminateContext(context.display, context.context);
        eglMakeCurrent(context.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        terminateDisplay(context.display);
    }

    terminateWindow();
}

class EGLDisplaySelectionTestVulkanDeviceUuid : public EGLDisplaySelectionTestNoFixture
{

  protected:
    static constexpr size_t kVulkanUuidSize = 16;
    using VulkanUuid                        = std::array<uint8_t, kVulkanUuidSize>;

    static VulkanUuid MakeUuid(uint8_t seed)
    {
        VulkanUuid uuid;
        uuid.fill(seed);
        return uuid;
    }

    // Deliberately does not call eglInitialize.  Display creation is lazy, so no
    // physical device is selected here and the test does not depend on the
    // running system having a device that matches |deviceUuid|.  The cache key
    // is built by eglGetPlatformDisplay alone, which is exactly what is under
    // test.
    EGLDisplay getDisplayWithDeviceUuid(const VulkanUuid &deviceUuid)
    {
        std::vector<EGLAttrib> displayAttributes;
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_VULKAN_DEVICE_UUID_ANGLE);
        displayAttributes.push_back(reinterpret_cast<EGLAttrib>(deviceUuid.data()));
        displayAttributes.push_back(EGL_NONE);

        EGLDisplay display = eglGetPlatformDisplay(
            GetEglPlatform(), reinterpret_cast<void *>(mOSWindow->getNativeDisplay()),
            displayAttributes.data());
        EXPECT_EGL_SUCCESS();
        return display;
    }
};

// Test that displays requesting different Vulkan device UUIDs are distinct, and
// that a UUID supplied through a different buffer still maps to the display
// already cached for those bytes.
TEST_P(EGLDisplaySelectionTestVulkanDeviceUuid, DistinctDeviceUuidsAreNotShared)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_platform_angle_vulkan_device_uuid"));

    initializeWindow();

    constexpr size_t kDisplayCount = 4;

    // Built up front so that no reallocation can move the bytes a display was
    // keyed on: the UUID must outlive the display that references it.
    std::vector<VulkanUuid> uuids;
    uuids.reserve(kDisplayCount);
    for (size_t i = 0; i < kDisplayCount; i++)
    {
        uuids.push_back(MakeUuid(static_cast<uint8_t>(i + 1)));
    }

    std::set<EGLDisplay> uniqueDisplays;
    std::vector<EGLDisplay> displays;
    for (const VulkanUuid &uuid : uuids)
    {
        EGLDisplay display = getDisplayWithDeviceUuid(uuid);
        ASSERT_TRUE(display != EGL_NO_DISPLAY);

        // Test that this display is unique
        EXPECT_TRUE(uniqueDisplays.insert(display).second);

        displays.push_back(display);
    }

    // Test that the key holds the UUID bytes rather than the caller's pointer,
    // by requesting the same UUIDs again through a separate buffer.
    for (size_t i = 0; i < kDisplayCount; i++)
    {
        const VulkanUuid sameBytesOtherBuffer = uuids[i];
        EXPECT_EQ(displays[i], getDisplayWithDeviceUuid(sameBytesOtherBuffer));
    }

    // Test mutating UUID contents in-place with the same buffer address:
    // the mutated contents must produce a new unique display.
    VulkanUuid mutableUuid           = MakeUuid(0xFE);
    EGLDisplay displayBeforeMutation = getDisplayWithDeviceUuid(mutableUuid);
    ASSERT_TRUE(displayBeforeMutation != EGL_NO_DISPLAY);
    EXPECT_TRUE(uniqueDisplays.insert(displayBeforeMutation).second);

    mutableUuid[0] += 1;
    EGLDisplay displayAfterMutation = getDisplayWithDeviceUuid(mutableUuid);
    ASSERT_TRUE(displayAfterMutation != EGL_NO_DISPLAY);
    EXPECT_TRUE(uniqueDisplays.insert(displayAfterMutation).second);

    for (EGLDisplay display : uniqueDisplays)
    {
        terminateDisplay(display);
    }

    terminateWindow();
}

// Test that the caller's UUID buffer does not have to outlive
// eglGetPlatformDisplay.  The physical device is chosen later, during
// eglInitialize, and must be chosen from the display's own copy of the bytes.
// Reading the caller's buffer there is a use-after-free that only a sanitizer
// build reports, so this test guards the contract rather than demonstrating a
// visible failure.
TEST_P(EGLDisplaySelectionTestVulkanDeviceUuid, DeviceUuidNeedNotOutliveDisplayCreation)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_platform_angle_vulkan_device_uuid"));

    initializeWindow();

    EGLDisplay display = EGL_NO_DISPLAY;
    {
        // Heap allocated so that a sanitizer poisons the bytes when the buffer
        // goes out of scope, the way a caller's stack array would be reused.
        std::vector<uint8_t> deviceUuid(kVulkanUuidSize, 0xAB);

        std::vector<EGLAttrib> displayAttributes;
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_VULKAN_DEVICE_UUID_ANGLE);
        displayAttributes.push_back(reinterpret_cast<EGLAttrib>(deviceUuid.data()));
        displayAttributes.push_back(EGL_NONE);

        display = eglGetPlatformDisplay(GetEglPlatform(),
                                        reinterpret_cast<void *>(mOSWindow->getNativeDisplay()),
                                        displayAttributes.data());
        ASSERT_EGL_SUCCESS();
    }
    ASSERT_TRUE(display != EGL_NO_DISPLAY);

    // No device carries this UUID, so the implementation falls back to another
    // device and initialization still succeeds.
    EXPECT_TRUE(eglInitialize(display, nullptr, nullptr) == EGL_TRUE);
    EXPECT_EGL_SUCCESS();

    terminateDisplay(display);
    terminateWindow();
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLDisplaySelectionTest);
ANGLE_INSTANTIATE_TEST(EGLDisplaySelectionTest,
                       WithLowPowerGPU(ES2_METAL()),
                       WithLowPowerGPU(ES3_METAL()),
                       WithHighPowerGPU(ES2_METAL()),
                       WithHighPowerGPU(ES3_METAL()));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLDisplaySelectionTestMultiDisplay);
ANGLE_INSTANTIATE_TEST(EGLDisplaySelectionTestMultiDisplay,
                       WithNoFixture(ES2_METAL()),
                       WithNoFixture(ES3_METAL()));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLDisplaySelectionTestDeviceId);
ANGLE_INSTANTIATE_TEST(EGLDisplaySelectionTestDeviceId,
                       WithNoFixture(ES2_D3D11()),
                       WithNoFixture(ES3_D3D11()),
                       WithNoFixture(ES2_METAL()),
                       WithNoFixture(ES3_METAL()),
                       WithNoFixture(WithLowPowerGPU(ES2_METAL())),
                       WithNoFixture(WithLowPowerGPU(ES3_METAL())),
                       WithNoFixture(WithHighPowerGPU(ES2_METAL())),
                       WithNoFixture(WithHighPowerGPU(ES3_METAL())));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLDisplaySelectionTestDisplayKey);
ANGLE_INSTANTIATE_TEST(EGLDisplaySelectionTestDisplayKey,
                       WithNoFixture(ES2_D3D11()),
                       WithNoFixture(ES3_D3D11()),
                       WithNoFixture(ES2_METAL()),
                       WithNoFixture(ES3_METAL()),
                       WithNoFixture(ES2_WEBGPU()),
                       WithNoFixture(ES3_WEBGPU()),
                       WithNoFixture(WithLowPowerGPU(ES2_METAL())),
                       WithNoFixture(WithLowPowerGPU(ES3_METAL())),
                       WithNoFixture(WithHighPowerGPU(ES2_METAL())),
                       WithNoFixture(WithHighPowerGPU(ES3_METAL())));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLDisplaySelectionTestVulkanDeviceUuid);
ANGLE_INSTANTIATE_TEST(EGLDisplaySelectionTestVulkanDeviceUuid,
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_VULKAN()));
