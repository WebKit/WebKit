//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "util/OSWindow.h"

using namespace angle;

namespace
{
// The sync entry point that the *RacesMakeCurrentAfterTerminate tests keep calling.
enum class SyncEntryPoint
{
    // eglClientWaitSync()
    ClientWait,
    // eglGetSyncAttribKHR()
    GetAttrib,
    // eglLabelObjectKHR(), which, unlike the other two, locks the display.
    Label,
};

// Returns false once the sync is no longer valid.
bool CallSyncEntryPoint(EGLDisplay display, EGLSync sync, SyncEntryPoint entryPoint)
{
    switch (entryPoint)
    {
        case SyncEntryPoint::ClientWait:
            // The reusable sync is never signaled, and a zero timeout returns immediately.
            return eglClientWaitSync(display, sync, 0, 0) == EGL_TIMEOUT_EXPIRED;
        case SyncEntryPoint::GetAttrib:
        {
            EGLint status = 0;
            return eglGetSyncAttribKHR(display, sync, EGL_SYNC_STATUS_KHR, &status) == EGL_TRUE;
        }
        case SyncEntryPoint::Label:
            return eglLabelObjectKHR(display, EGL_OBJECT_SYNC_KHR, sync, nullptr) == EGL_SUCCESS;
    }
    return false;
}
}  // anonymous namespace

class EGLDisplayTest : public ANGLETest<>
{
  protected:
    void testSyncCallsRaceMakeCurrentAfterTerminate(SyncEntryPoint entryPoint);

    EGLConfig chooseConfig(EGLDisplay display)
    {
        const EGLint attribs[] = {EGL_RED_SIZE,
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
                                  EGL_PBUFFER_BIT | EGL_WINDOW_BIT,
                                  EGL_NONE};
        EGLConfig config       = EGL_NO_CONFIG_KHR;
        EGLint count           = 0;
        EXPECT_EGL_TRUE(eglChooseConfig(display, attribs, &config, 1, &count));
        EXPECT_EGL_TRUE(count > 0);
        return config;
    }

    EGLContext createContext(EGLDisplay display, EGLConfig config)
    {
        const EGLint attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_NONE};
        EGLContext context     = eglCreateContext(display, config, nullptr, attribs);
        EXPECT_NE(context, EGL_NO_CONTEXT);
        return context;
    }

    EGLSurface createPBufferSurface(EGLDisplay display, EGLConfig config)
    {
        const EGLint attribs[] = {EGL_WIDTH, kDefaultPBufferSize, EGL_HEIGHT, kDefaultPBufferSize,
                                  EGL_NONE};
        EGLSurface surface     = eglCreatePbufferSurface(display, config, attribs);
        EXPECT_NE(surface, EGL_NO_SURFACE);
        return surface;
    }

    static constexpr EGLint kDefaultPBufferSize = 64;
};

class EGLDisplayTestES3 : public EGLDisplayTest
{};

// Tests that an eglInitialize can be re-initialized.  The spec says:
//
// > Initializing an already-initialized display is allowed, but the only effect of such a call is
// to return EGL_TRUE and update the EGL version numbers
TEST_P(EGLDisplayTest, InitializeMultipleTimes)
{
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0, minor = 0;
    EXPECT_EGL_TRUE(eglInitialize(display, &major, &minor) != EGL_FALSE);
    for (uint32_t i = 0; i < 10; ++i)
    {
        EGLint retryMajor = 123456, retryMinor = -1;
        EXPECT_EGL_TRUE(eglInitialize(display, &retryMajor, &retryMinor) != EGL_FALSE);
        EXPECT_EQ(major, retryMajor) << i;
        EXPECT_EQ(minor, retryMinor) << i;
    }
}

// Test that call eglInitialize() in parallel in multiple threads works
// > Initializing an already-initialized display is allowed, but the only effect
// of such a call is to return EGL_TRUE and update the EGL version numbers
TEST_P(EGLDisplayTest, InitializeMultipleTimesInDifferentThreads)
{
    std::array<std::thread, 10> threads;
    for (std::thread &thread : threads)
    {
        thread = std::thread([&]() {
            EGLDisplay display                 = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            const int INVALID_GL_MAJOR_VERSION = -1;
            const int INVALID_GL_MINOR_VERSION = -1;
            EGLint threadMajor                 = INVALID_GL_MAJOR_VERSION;
            EGLint threadMinor                 = INVALID_GL_MINOR_VERSION;
            EXPECT_EGL_TRUE(eglInitialize(display, &threadMajor, &threadMinor) != EGL_FALSE);
            EXPECT_NE(threadMajor, INVALID_GL_MAJOR_VERSION);
            EXPECT_NE(threadMinor, INVALID_GL_MINOR_VERSION);
        });
    }

    for (std::thread &thread : threads)
    {
        thread.join();
    }

    // Properly clean up so it won't affect next test
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EXPECT_EGL_TRUE(eglTerminate(display) != EGL_FALSE);
}

// Test that calling eglTerminate() in parallel in multiple threads works
TEST_P(EGLDisplayTest, TerminateMultipleTimesInDifferentThreads)
{
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EXPECT_EGL_TRUE(eglInitialize(display, nullptr, nullptr) != EGL_FALSE);

    std::array<std::thread, 10> threads;
    for (std::thread &thread : threads)
    {
        thread = std::thread([&]() { EXPECT_EGL_TRUE(eglTerminate(display) != EGL_FALSE); });
    }

    for (std::thread &thread : threads)
    {
        thread.join();
    }
}

// Tests that an EGLDisplay can be re-initialized.
TEST_P(EGLDisplayTest, InitializeTerminateInitialize)
{
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EXPECT_EGL_TRUE(eglInitialize(display, nullptr, nullptr) != EGL_FALSE);
    EXPECT_EGL_TRUE(eglTerminate(display) != EGL_FALSE);
    EXPECT_EGL_TRUE(eglInitialize(display, nullptr, nullptr) != EGL_FALSE);

    // Properly clean up so it won't affect next test
    EXPECT_EGL_TRUE(eglTerminate(display) != EGL_FALSE);
}

// Tests that an EGLDisplay can be re-initialized after it was used to draw into a window surface.
TEST_P(EGLDisplayTest, InitializeDrawSwapTerminateLoop)
{
    constexpr int kLoopCount = 2;
    constexpr EGLint kWidth  = 64;
    constexpr EGLint kHeight = 64;

    OSWindow *osWindow = OSWindow::New();
    osWindow->initialize("LockSurfaceTest", kWidth, kHeight);

    // Not EGL_DEFAULT_DISPLAY: on Wayland the implementation would connect for itself, and that
    // connection cannot own the window's wl_surface. anglebug.com/546514492
    EGLAttrib dispattrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, GetParam().getRenderer(),
                             EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE,
                             static_cast<EGLAttrib>(osWindow->getNativeDisplayPlatformType()),
                             EGL_NONE};
    EGLDisplay display    = eglGetPlatformDisplay(
        GetEglPlatform(), reinterpret_cast<void *>(osWindow->getNativeDisplay()), dispattrs);
    EXPECT_NE(display, EGL_NO_DISPLAY);

    for (int i = 0; i < kLoopCount; ++i)
    {
        EXPECT_EGL_TRUE(eglInitialize(display, nullptr, nullptr) != EGL_FALSE);

        EGLConfig config   = chooseConfig(display);
        EGLContext context = createContext(display, config);
        EGLSurface surface =
            eglCreateWindowSurface(display, config, osWindow->getNativeWindow(), nullptr);
        EXPECT_NE(surface, EGL_NO_SURFACE);

        EXPECT_EGL_TRUE(eglMakeCurrent(display, surface, surface, context));

        ANGLE_GL_PROGRAM(greenProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
        drawQuad(greenProgram, essl1_shaders::PositionAttrib(), 0.5f);

        EXPECT_EGL_TRUE(eglSwapBuffers(display, surface));

        EXPECT_EGL_TRUE(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        EXPECT_EGL_TRUE(eglTerminate(display) != EGL_FALSE);
    }

    osWindow->destroy();
    OSWindow::Delete(&osWindow);
}

// Tests current Context leaking when call eglTerminate() while it is current.
TEST_P(EGLDisplayTest, ContextLeakAfterTerminate)
{
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EXPECT_EGL_TRUE(eglInitialize(display, nullptr, nullptr));

    EGLConfig config   = chooseConfig(display);
    EGLContext context = createContext(display, config);
    EGLSurface surface = createPBufferSurface(display, config);

    // Make "context" current.
    EXPECT_EGL_TRUE(eglMakeCurrent(display, surface, surface, context));

    // Terminate display while "context" is current.
    EXPECT_EGL_TRUE(eglTerminate(display));

    // Unmake "context" from current and allow Display to actually terminate.
    (void)eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    // Get EGLDisplay again.
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    // Check if Display was actually terminated.
    EGLint val;
    EXPECT_EGL_FALSE(eglQueryContext(display, context, EGL_CONTEXT_CLIENT_TYPE, &val));
    EXPECT_EQ(eglGetError(), EGL_NOT_INITIALIZED);
}

// Tests that releasing the current context with eglMakeCurrent(dpy, EGL_NO_SURFACE,
// EGL_NO_SURFACE, EGL_NO_CONTEXT) concurrently with eglTerminate(dpy) on another thread does not
// race with display termination or leave the display unterminated.
TEST_P(EGLDisplayTest, ReleaseCurrentContextConcurrentWithTerminate)
{
    EGLAttrib dispattrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                             GetParam().getRenderer(),
                             EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
                             GetParam().getDeviceType(),
                             EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE,
                             static_cast<EGLAttrib>(GetPbufferOnlyDefaultPlatformType()),
                             EGL_NONE};

    constexpr int kIterations = 20;

    for (int i = 0; i < kIterations; ++i)
    {
        EGLDisplay display = eglGetPlatformDisplay(
            GetEglPlatform(), reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
        ASSERT_NE(display, EGL_NO_DISPLAY);

        EGLContext context = EGL_NO_CONTEXT;
        EGLSurface surface = EGL_NO_SURFACE;
        std::atomic<bool> contextReady{false};

        std::thread contextThread([&]() {
            ASSERT_EGL_TRUE(eglInitialize(display, nullptr, nullptr));

            EGLConfig config      = chooseConfig(display);
            GLint maxPBufferWidth = 0;
            EXPECT_EGL_TRUE(
                eglGetConfigAttrib(display, config, EGL_MAX_PBUFFER_WIDTH, &maxPBufferWidth));
            if (maxPBufferWidth < kDefaultPBufferSize)
            {
                contextReady.store(true, std::memory_order_release);
                return;
            }

            context = createContext(display, config);
            surface = createPBufferSurface(display, config);

            EXPECT_EGL_TRUE(eglMakeCurrent(display, surface, surface, context));
            if (i % 2 == 1)
            {
                EXPECT_EGL_TRUE(eglDestroyContext(display, context));
            }
            glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // Whether context is destroyed or not, it should not produce a GL error.
            EXPECT_GL_NO_ERROR();

            contextReady.store(true, std::memory_order_release);

            // Poll eglQuerySurface until eglTerminate sets kTerminatingBit on the display and waits
            // in waitUntilUnreferenced(1).
            EGLint width = 0;
            while (eglQuerySurface(display, surface, EGL_WIDTH, &width) == EGL_TRUE)
            {
            }

            EXPECT_EGL_TRUE(
                eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        });

        std::thread terminateThread([&]() {
            while (!contextReady.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            EXPECT_EGL_TRUE(eglTerminate(display));
        });

        contextThread.join();
        terminateThread.join();

        // Both eglTerminate and eglMakeCurrent(EGL_NO_CONTEXT) have completed, so the display
        // must be fully terminated.
        EGLint val = 0;
        EXPECT_EGL_FALSE(eglQueryContext(display, context, EGL_CONTEXT_CLIENT_TYPE, &val));
        EXPECT_EQ(eglGetError(), EGL_NOT_INITIALIZED);
    }
}

// Tests that eglMakeCurrent with bad display pointer fails but doesn't crash.
TEST_P(EGLDisplayTest, MakeCurrentWithBadDisplay)
{
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EXPECT_EGL_TRUE(eglInitialize(display, nullptr, nullptr));

    EGLConfig config   = chooseConfig(display);
    EGLContext context = createContext(display, config);

    // Call eglMakeCurrent with bad display pointer. It should fail validation but not crash.
    EXPECT_EGL_FALSE(eglMakeCurrent(EGL_CAST(EGLDisplay, (void *)0xdeadbeef), EGL_NO_SURFACE,
                                    EGL_NO_SURFACE, context));
    EXPECT_EQ(eglGetError(), EGL_BAD_DISPLAY);

    // Call eglMakeCurrent with no display. It should fail validation but not crash.
    EXPECT_EGL_FALSE(eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, context));
    EXPECT_EQ(eglGetError(), EGL_BAD_DISPLAY);

    EXPECT_EGL_TRUE(eglDestroyContext(display, context));
    EXPECT_EGL_TRUE(eglTerminate(display));
}

// Tests eglGetPlatformDisplayEXT() when EGL_EXT_platform_base is enabled.
TEST_P(EGLDisplayTest, GetPlatformDisplayEXT)
{
    // eglGetPlatformDisplayEXT() requires EGL_EXT_platform_base.
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_EXT_platform_base"));

#if !defined(EGL_EGLEXT_PROTOTYPES) || !EGL_EGLEXT_PROTOTYPES
    ASSERT_TRUE(eglGetPlatformDisplayEXT != nullptr);
#endif

    EGLint dispattrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, GetParam().getRenderer(), EGL_NONE};
    EGLDisplay display = eglGetPlatformDisplayEXT(
        GetEglPlatform(), reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);

    ASSERT_NE(EGL_NO_DISPLAY, display);
    ASSERT_EGL_SUCCESS();
}

// Tests eglQueryString(EGL_EXTENSIONS) with EGL_BAD_DISPLAY should not crash
TEST_P(EGLDisplayTest, QueryEGLExtensionWithBadDisplay)
{
    eglQueryString(EGL_CAST(EGLDisplay, (void *)0xdeadbeef), EGL_EXTENSIONS);
    EXPECT_EQ(eglGetError(), EGL_BAD_DISPLAY);
}

// Tests eglQueryString(EGL_EXTENSIONS) with EGL_NO_DISPLAY should succeed without error
TEST_P(EGLDisplayTest, QueryEGLExtensionWithNoDisplay)
{
    eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    ASSERT_EGL_SUCCESS();
}

// Tests eglQueryString with EGL_NO_DISPLAY should fail EGL_BAD_DISPLAY error
TEST_P(EGLDisplayTest, QueryEGLVendorWithBadDisplay)
{
    eglQueryString(EGL_NO_DISPLAY, EGL_VENDOR);
    EXPECT_EQ(eglGetError(), EGL_BAD_DISPLAY);
}

// Tests that EGL_ANGLE_query_renderer_strings returns the same strings as the GL context.
TEST_P(EGLDisplayTest, QueryRendererStrings)
{
    EGLAttrib dispattrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, GetParam().getRenderer(), EGL_NONE};
    EGLDisplay display    = eglGetPlatformDisplay(
        GetEglPlatform(), reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
    ASSERT_NE(display, EGL_NO_DISPLAY);

    ASSERT_EGL_TRUE(eglInitialize(display, nullptr, nullptr));

    if (!IsEGLDisplayExtensionEnabled(display, "EGL_ANGLE_query_renderer_strings"))
    {
        EXPECT_EGL_TRUE(eglTerminate(display));
        ANGLE_SKIP_TEST_IF(true);
    }

    const char *rendererString = eglQueryString(display, EGL_RENDERER_STRING_ANGLE);
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(nullptr, rendererString);
    const char *versionString = eglQueryString(display, EGL_ANGLE_VERSION_STRING_ANGLE);
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(nullptr, versionString);

    // The returned strings match the ones of a (non-WebGL) context created on the display.
    EGLConfig config   = chooseConfig(display);
    EGLContext context = createContext(display, config);
    EGLSurface surface = createPBufferSurface(display, config);
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, context));

    EXPECT_STREQ(reinterpret_cast<const char *>(glGetString(GL_RENDERER)), rendererString);
    const std::string glVersion = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    EXPECT_NE(std::string::npos, glVersion.find(std::string("(ANGLE ") + versionString + ")"))
        << glVersion;

    EXPECT_EGL_TRUE(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    EXPECT_EGL_TRUE(eglDestroySurface(display, surface));
    EXPECT_EGL_TRUE(eglDestroyContext(display, context));
    EXPECT_EGL_TRUE(eglTerminate(display));
}

// Tests that eglGetPlatformDisplayEXT can use EGL_PLATFORM_ANDROID_KHR to get a display.
TEST_P(EGLDisplayTestES3, GetPlatformDisplayAndroidValidation)
{
    ANGLE_SKIP_TEST_IF(!IsAndroid());
    ANGLE_SKIP_TEST_IF(getDriverType() != GLESDriverType::SystemEGL);

    // Get an EGLDisplay on GBM platform, expect EGL_BAD_PARAMETER
    EGLDisplay display1 = eglGetPlatformDisplay(
        EGL_PLATFORM_GBM_KHR, reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), nullptr);
    ASSERT_EQ(EGL_NO_DISPLAY, display1);
    ASSERT_EGL_ERROR(EGL_BAD_PARAMETER);

    // Get an EGLDisplay on Android platform, expect EGL_SUCCESS
    EGLDisplay display2 = eglGetPlatformDisplay(
        EGL_PLATFORM_ANDROID_KHR, reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), nullptr);
    ASSERT_NE(EGL_NO_DISPLAY, display2);
    ASSERT_EGL_SUCCESS();
}

// Calls eglTerminate() while contexts are still current, then releases them one at a time on
// separate threads while other threads keep calling |entryPoint| on syncs.  Each release after
// eglTerminate() cleans up the display, which must not deadlock with the sync calls; if it does,
// the test times out.
void EGLDisplayTest::testSyncCallsRaceMakeCurrentAfterTerminate(SyncEntryPoint entryPoint)
{
    // Each releasing thread gives the sync threads one chance to win the race, which they do about
    // once in every 5 to 10 tries.
    constexpr size_t kReleasingThreadCount = 32;
    // Calls to eglLabelObjectKHR() are serialized by the display mutex, so more threads would only
    // slow them down.
    const size_t syncThreadCount = entryPoint == SyncEntryPoint::Label ? 1 : 8;
    // A thread that is stuck for this long is not coming back.
    constexpr std::chrono::seconds kTimeout(20);

    const EGLAttrib displayAttribs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, GetParam().getRenderer(),
                                        EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE,
                                        static_cast<EGLAttrib>(GetPbufferOnlyDefaultPlatformType()),
                                        EGL_NONE};
    EGLDisplay display               = eglGetPlatformDisplay(
        GetEglPlatform(), reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), displayAttribs);
    ASSERT_NE(display, EGL_NO_DISPLAY);
    ASSERT_EGL_TRUE(eglInitialize(display, nullptr, nullptr));

    const bool hasExtensions =
        IsEGLDisplayExtensionEnabled(display, "EGL_KHR_surfaceless_context") &&
        IsEGLDisplayExtensionEnabled(display, "EGL_KHR_reusable_sync") &&
        (entryPoint != SyncEntryPoint::Label || IsEGLClientExtensionEnabled("EGL_KHR_debug"));
    if (!hasExtensions)
    {
        EXPECT_EGL_TRUE(eglTerminate(display));
    }
    ANGLE_SKIP_TEST_IF(!hasExtensions);

    // This thread's context stays current throughout, which keeps the display initialized after
    // eglTerminate().
    EGLConfig config         = chooseConfig(display);
    EGLContext renderContext = createContext(display, config);
    ASSERT_EGL_TRUE(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, renderContext));

    // On the GL backends, contexts share a native context unless they are in different
    // virtualization groups, and a native context cannot be current on two threads at once.  The
    // releasing threads therefore put each of their contexts in a group of its own.
    const bool hasContextVirtualization =
        IsEGLDisplayExtensionEnabled(display, "EGL_ANGLE_context_virtualization");
    // Must be after eglMakeCurrent() so that the renderer string is available.
    const bool supportsMultithreading = hasContextVirtualization || IsVulkan();
    if (!supportsMultithreading)
    {
        EXPECT_EGL_TRUE(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        EXPECT_EGL_TRUE(eglTerminate(display));
    }
    ANGLE_SKIP_TEST_IF(!supportsMultithreading);

    std::mutex mutex;
    std::condition_variable condVar;

    // The following are protected by |mutex|.
    // Releasing threads that are done trying to make their context current.
    size_t setUpCount = 0;
    bool setUpFailed  = false;
    // Releasing threads that may release their context, and that have done so.
    size_t releaseCount  = 0;
    size_t releasedCount = 0;
    // The syncs that the sync threads use in the current round, and how many have started.
    size_t syncRound = 0;
    std::vector<EGLSync> roundSyncs;
    size_t syncThreadsStarted = 0;
    bool stopSyncThreads      = false;

    std::atomic<bool> stop(false);

    auto waitFor = [&](std::unique_lock<std::mutex> &lock, auto predicate, const char *what) {
        if (!condVar.wait_for(lock, kTimeout, predicate))
        {
            // The stuck thread cannot be unwound, so there is no way to carry on.
            FATAL() << "Timed out waiting for " << what
                    << "; a thread is deadlocked in the deferred cleanup of the terminated "
                       "display.";
        }
    };

    auto createSync = [&]() {
        const auto deadline = std::chrono::steady_clock::now() + kTimeout;
        std::chrono::milliseconds backoff(1);
        while (true)
        {
            EGLSyncKHR sync = eglCreateSyncKHR(display, EGL_SYNC_REUSABLE_KHR, nullptr);
            if (sync != EGL_NO_SYNC_KHR)
            {
                return static_cast<EGLSync>(sync);
            }
            if (std::chrono::steady_clock::now() > deadline)
            {
                FATAL() << "The display never finished terminating; a thread is stuck in the "
                           "deferred cleanup of the terminated display.";
            }
            std::this_thread::sleep_for(backoff);
            backoff = std::min(backoff * 2, std::chrono::milliseconds(1000));
        }
    };

    std::vector<std::thread> releasingThreads;
    for (size_t index = 0; index < kReleasingThreadCount; ++index)
    {
        releasingThreads.emplace_back([&, index]() {
            // Group 0 and up are all different from the default group of |renderContext|.
            EGLint contextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 2,
                                       EGL_CONTEXT_VIRTUALIZATION_GROUP_ANGLE,
                                       static_cast<EGLint>(index), EGL_NONE};
            if (!hasContextVirtualization)
            {
                contextAttribs[2] = EGL_NONE;
            }
            EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
            const bool isCurrent =
                context != EGL_NO_CONTEXT &&
                eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context) == EGL_TRUE;
            EXPECT_TRUE(isCurrent);

            {
                std::unique_lock<std::mutex> lock(mutex);
                ++setUpCount;
                setUpFailed = setUpFailed || !isCurrent;
                condVar.notify_all();

                condVar.wait(lock, [&]() { return releaseCount > index; });
            }

            // After eglTerminate(), this schedules the deferred cleanup.
            EXPECT_EGL_TRUE(
                eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

            {
                std::unique_lock<std::mutex> lock(mutex);
                ++releasedCount;
                condVar.notify_all();
            }
        });
    }

    std::vector<std::thread> syncThreads;
    for (size_t index = 0; index < syncThreadCount; ++index)
    {
        syncThreads.emplace_back([&, index]() {
            size_t lastRound = 0;
            while (true)
            {
                EGLSync sync = EGL_NO_SYNC;
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    condVar.wait(lock, [&]() { return stopSyncThreads || syncRound != lastRound; });
                    if (stopSyncThreads)
                    {
                        return;
                    }
                    lastRound = syncRound;
                    sync      = roundSyncs[index];
                    ++syncThreadsStarted;
                    condVar.notify_all();
                }

                // Keep calling the entry point until a cleanup invalidates the sync.
                while (!stop.load(std::memory_order_relaxed) &&
                       CallSyncEntryPoint(display, sync, entryPoint))
                {
                }
            }
        });
    }

    {
        std::unique_lock<std::mutex> lock(mutex);
        waitFor(
            lock, [&]() { return setUpCount == kReleasingThreadCount; },
            "the releasing threads to make their context current");
    }

    if (!setUpFailed)
    {
        // Every releasing thread and this thread have a context current, so the display stays
        // initialized.  From now on, every context switch schedules the deferred cleanup.
        EXPECT_EGL_TRUE(eglTerminate(display));

        // The extra round at the end has nothing to release; it only checks that the sync threads
        // made it back from the last release.
        for (size_t round = 0; round <= kReleasingThreadCount; ++round)
        {
            std::vector<EGLSync> syncs(syncThreadCount);
            for (EGLSync &sync : syncs)
            {
                sync = createSync();
            }

            std::unique_lock<std::mutex> lock(mutex);
            roundSyncs         = std::move(syncs);
            syncThreadsStarted = 0;
            ++syncRound;
            condVar.notify_all();
            waitFor(
                lock, [&]() { return syncThreadsStarted == syncThreadCount; },
                "the sync threads to start using new syncs");

            if (round == kReleasingThreadCount)
            {
                break;
            }

            releaseCount = round + 1;
            condVar.notify_all();
            waitFor(
                lock, [&]() { return releasedCount == round + 1; },
                "a releasing thread to return from eglMakeCurrent()");
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        releaseCount    = kReleasingThreadCount;
        stopSyncThreads = true;
    }
    stop = true;
    condVar.notify_all();

    for (std::thread &thread : syncThreads)
    {
        thread.join();
    }
    for (std::thread &thread : releasingThreads)
    {
        thread.join();
    }

    // Releasing the last current context completes the termination.
    EXPECT_EGL_TRUE(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    EXPECT_EGL_TRUE(eglTerminate(display));
}

// Test that eglClientWaitSync() does not deadlock when it races with eglMakeCurrent() after
// eglTerminate().
TEST_P(EGLDisplayTest, ClientWaitSyncRacesMakeCurrentAfterTerminate)
{
    testSyncCallsRaceMakeCurrentAfterTerminate(SyncEntryPoint::ClientWait);
}

// Test that eglGetSyncAttribKHR() does not deadlock when it races with eglMakeCurrent() after
// eglTerminate().
TEST_P(EGLDisplayTest, GetSyncAttribRacesMakeCurrentAfterTerminate)
{
    testSyncCallsRaceMakeCurrentAfterTerminate(SyncEntryPoint::GetAttrib);
}

// Test that eglLabelObjectKHR() on a sync does not deadlock when it races with eglMakeCurrent()
// after eglTerminate().
TEST_P(EGLDisplayTest, LabelSyncRacesMakeCurrentAfterTerminate)
{
    testSyncCallsRaceMakeCurrentAfterTerminate(SyncEntryPoint::Label);
}

// eglClientWaitSync() waits for a pending fence in an unlocked tail call, after it has released its
// locks.  The tail call uses the display, both to wait through the renderer and to release the
// sync, so the display must stay referenced until the tail call is done.  Otherwise, nothing stops
// the deferred cleanup of a terminated display, which another thread's eglMakeCurrent() runs, from
// terminating the display for real while the tail call still waits through the renderer, or before
// it releases the sync.
//
// Here, a few threads keep waiting on a pending fence with a non-zero timeout, which takes the tail
// call path, while this thread releases the last context that was current across eglTerminate().
// The deferred cleanup that this schedules then terminates the display.  This only happens once per
// initialization of the display, so it is repeated a number of times.
//
// The teardown only overlaps a tail call some of the time, so a tail call that runs while the
// display is not referenced only makes this test fail some of the time.  In builds with assertions,
// ScopedSyncRef also checks that its sync is released while the display is held, which fails as
// soon as a wait takes the tail call path.
TEST_P(EGLDisplayTestES3, ClientWaitSyncOnPendingFenceRacesLastMakeCurrentAfterTerminate)
{
    // The Vulkan backend waits for pending fences in unlocked tail calls.
    ANGLE_SKIP_TEST_IF(GetParam().getRenderer() != EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE);

    constexpr size_t kRoundCount      = 10;
    constexpr size_t kWaitThreadCount = 8;
    // A thread that is stuck for this long is not coming back.
    constexpr std::chrono::seconds kTimeout(20);
    // Any non-zero timeout makes eglClientWaitSync() wait for a pending fence in a tail call.  The
    // shortest one keeps the threads coming back for more.
    constexpr EGLTime kWaitTimeoutNs = 1;
    // The fence should still be pending when this thread releases its context.  If it is not, the
    // next round draws more.
    constexpr GLsizei kFramebufferSize = 256;
    constexpr size_t kMaxDrawCount     = 64;

    constexpr char kVS[] = R"(attribute vec2 position;
void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
})";

    // Keeps the GPU busy for a while.
    constexpr char kFS[] = R"(precision highp float;
uniform float seed;
void main()
{
    float value = gl_FragCoord.x * seed + gl_FragCoord.y;
    for (int i = 0; i < 256; ++i)
    {
        value = fract(sin(value) * 43758.5453);
    }
    gl_FragColor = vec4(value);
})";

    // The validation layers serialize the fence waits of the waiting threads, which makes them a
    // lot less likely to run into the deferred cleanup.
    const EGLAttrib displayAttribs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                                        GetParam().getRenderer(),
                                        EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE,
                                        static_cast<EGLAttrib>(GetPbufferOnlyDefaultPlatformType()),
                                        EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE,
                                        EGL_FALSE,
                                        EGL_NONE};
    EGLDisplay display               = eglGetPlatformDisplay(
        GetEglPlatform(), reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), displayAttribs);
    ASSERT_NE(display, EGL_NO_DISPLAY);
    EGLint major = 0;
    EGLint minor = 0;
    ASSERT_EGL_TRUE(eglInitialize(display, &major, &minor));

    // eglClientWaitSync() needs EGL 1.5, and eglClientWaitSyncKHR() EGL_KHR_fence_sync.
    const bool hasExtensions =
        (major > 1 || minor >= 5) &&
        IsEGLDisplayExtensionEnabled(display, "EGL_KHR_surfaceless_context") &&
        IsEGLDisplayExtensionEnabled(display, "EGL_KHR_fence_sync");
    if (!hasExtensions)
    {
        EXPECT_EGL_TRUE(eglTerminate(display));
    }
    ANGLE_SKIP_TEST_IF(!hasExtensions);

    std::mutex mutex;
    std::condition_variable condVar;
    size_t drawCount = 1;

    for (size_t round = 0; round < kRoundCount; ++round)
    {
        // The deferred cleanup of the previous round terminated the display.
        if (round > 0)
        {
            ASSERT_EGL_TRUE(eglInitialize(display, nullptr, nullptr));
        }

        EGLConfig config   = chooseConfig(display);
        EGLContext context = createContext(display, config);
        ASSERT_EGL_TRUE(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context));

        // |context| is still current, so the display stays initialized, and |context| remains
        // usable.  eglTerminate() invalidates the syncs that exist, so the fence is created after
        // it.
        ASSERT_EGL_TRUE(eglTerminate(display));

        EGLSync fence = EGL_NO_SYNC;
        {
            GLTexture texture;
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kFramebufferSize, kFramebufferSize, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, nullptr);
            GLFramebuffer framebuffer;
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
            ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
            glViewport(0, 0, kFramebufferSize, kFramebufferSize);

            ANGLE_GL_PROGRAM(program, kVS, kFS);
            glUseProgram(program);
            glUniform1f(glGetUniformLocation(program, "seed"), 1.0f);

            // A triangle that covers the whole framebuffer.
            const GLfloat vertices[]     = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
            const GLint positionLocation = glGetAttribLocation(program, "position");
            ASSERT_NE(positionLocation, -1);
            glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, vertices);
            glEnableVertexAttribArray(positionLocation);
            for (size_t draw = 0; draw < drawCount; ++draw)
            {
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }
            ASSERT_GL_NO_ERROR();

            // This also submits the draws.
            fence = eglCreateSync(display, EGL_SYNC_FENCE, nullptr);
            ASSERT_NE(fence, EGL_NO_SYNC);
        }

        // The following are protected by |mutex|.
        size_t startedCount = 0;
        size_t doneCount    = 0;

        std::vector<std::thread> waitThreads;
        for (size_t index = 0; index < kWaitThreadCount; ++index)
        {
            waitThreads.emplace_back([&, index]() {
                bool started = false;
                while (true)
                {
                    // Use both the EGL 1.5 and the EGL_KHR_fence_sync entry points.
                    const EGLint result =
                        index % 2 == 0 ? eglClientWaitSync(display, fence, 0, kWaitTimeoutNs)
                                       : eglClientWaitSyncKHR(display, fence, 0, kWaitTimeoutNs);
                    const EGLint error = eglGetError();

                    if (!started)
                    {
                        started = true;
                        std::lock_guard<std::mutex> lock(mutex);
                        ++startedCount;
                        condVar.notify_all();
                    }

                    if (result == EGL_FALSE)
                    {
                        // The display was terminated, along with the fence.
                        EXPECT_NE(error, EGL_SUCCESS)
                            << "eglClientWaitSync() failed without an error; the result of the "
                               "wait was lost.";
                        break;
                    }
                    EXPECT_TRUE(result == EGL_CONDITION_SATISFIED || result == EGL_TIMEOUT_EXPIRED)
                        << result;
                }

                std::lock_guard<std::mutex> lock(mutex);
                ++doneCount;
                condVar.notify_all();
            });
        }

        {
            std::unique_lock<std::mutex> lock(mutex);
            if (!condVar.wait_for(lock, kTimeout,
                                  [&]() { return startedCount == kWaitThreadCount; }))
            {
                FATAL() << "Timed out waiting for the threads to start waiting for the fence.";
            }
        }

        // Make the next round draw more if the fence is no longer pending.
        if (eglClientWaitSync(display, fence, 0, 0) != EGL_TIMEOUT_EXPIRED)
        {
            drawCount = std::min(drawCount * 2, kMaxDrawCount);
        }

        // This is the last context that is current, so the deferred cleanup that this schedules
        // terminates the display, while the other threads are waiting for the fence.
        EXPECT_EGL_TRUE(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

        {
            std::unique_lock<std::mutex> lock(mutex);
            if (!condVar.wait_for(lock, kTimeout, [&]() { return doneCount == kWaitThreadCount; }))
            {
                // The stuck thread cannot be unwound, so there is no way to carry on.
                FATAL() << "Timed out waiting for the threads to return from eglClientWaitSync(); "
                           "a thread is deadlocked in the deferred cleanup of the terminated "
                           "display.";
            }
        }
        for (std::thread &thread : waitThreads)
        {
            thread.join();
        }
    }
}

ANGLE_INSTANTIATE_TEST(EGLDisplayTest,
                       WithNoFixture(ES2_D3D11()),
                       WithNoFixture(ES2_METAL()),
                       WithNoFixture(ES2_OPENGL()),
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_D3D11()),
                       WithNoFixture(ES3_METAL()),
                       WithNoFixture(ES3_OPENGL()),
                       WithNoFixture(ES3_VULKAN()));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLDisplayTestES3);
ANGLE_INSTANTIATE_TEST(EGLDisplayTestES3, WithNoFixture(ES3_VULKAN()));
